#include "AIProcessor.h"
#include "AIProcessorONNX.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QMetaType>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrent>
#include <QDir>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QThread>
#include <QUuid>
#include <QSet>
#include <algorithm>
#include <mutex>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/core/cuda.hpp>
#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#endif
#include <cmath>

namespace {
const cv::Size kDnnInputSize(300, 300);
const cv::Scalar kMeanValues(104.0, 177.0, 123.0); // commonly used for FaceNet style models
const QString kDefaultEmbeddingsPath = QStringLiteral("config/embeddings.json");
constexpr int kMaxProcessWidth = 960; // downscale wide frames for faster inference while keeping quality

void configureOpenCvThreading()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, []() {
        cv::setUseOptimized(true);
        int ideal = QThread::idealThreadCount();
        const int workerThreads = std::max(1, (ideal > 0 ? ideal - 2 : 2));
        cv::setNumThreads(workerThreads);
        qInfo() << "OpenCV threads limited to" << workerThreads;
    });
}

QString resolvePath(const QString& path)
{
    if (QFileInfo(path).isAbsolute())
        return path;
    return QCoreApplication::applicationDirPath() + "/" + path;
}

QString sanitizeName(const QString& name)
{
    QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        trimmed = QStringLiteral("person");
    trimmed.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("_"));
    return trimmed.left(64);
}

void setDnnBackend(cv::dnn::Net& net)
{
    auto setCpu = [&]() {
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    };

    int gpuCount = 0;
    try {
        gpuCount = cv::cuda::getCudaEnabledDeviceCount();
    } catch (const cv::Exception&) {
        gpuCount = 0;
    }

    if (gpuCount > 0) {
        try {
            net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
            return;
        } catch (const cv::Exception& ex) {
            qWarning() << "CUDA backend not available, falling back to CPU:" << ex.what();
        }
    }

    setCpu();
}

struct ScaledFrame {
    cv::Mat image;
    float scale = 1.0f; // scaled_width = original_width * scale
};

ScaledFrame makeScaledFrame(const cv::Mat& frame, int maxWidth)
{
    ScaledFrame scaled;
    scaled.scale = 1.0f;
    if (frame.empty())
        return scaled;

    if (frame.cols > maxWidth) {
        scaled.scale = static_cast<float>(maxWidth) / static_cast<float>(frame.cols);
        cv::resize(frame, scaled.image, cv::Size(), scaled.scale, scaled.scale, cv::INTER_AREA);
    } else {
        scaled.image = frame;
    }
    return scaled;
}
}

AIProcessor::AIProcessor(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<Detection>("Detection");
    qRegisterMetaType<QVector<Detection>>("QVector<Detection>");
    qRegisterMetaType<FaceProfile>("AIProcessor::FaceProfile");
    qRegisterMetaType<QVector<FaceProfile>>("QVector<AIProcessor::FaceProfile>");

    configureOpenCvThreading();
    embedEngine = std::make_unique<AIProcessorONNX>();
    personHog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());
    loadKnownEmbeddings(kDefaultEmbeddingsPath);
    autoEnrollTimer.start();
}

bool AIProcessor::loadFaceModel(const QString& modelPath, const QString& configPath)
{
    try {
        if (configPath.isEmpty())
            faceNet = cv::dnn::readNet(modelPath.toStdString());
        else
            faceNet = cv::dnn::readNet(modelPath.toStdString(), configPath.toStdString());
        if (!faceNet.empty())
            setDnnBackend(faceNet);
        return !faceNet.empty();
    } catch (const cv::Exception& ex) {
        qWarning() << "Failed to load face model:" << ex.what();
        return false;
    }
}

bool AIProcessor::loadObjectModel(const QString& modelPath, const QString& configPath)
{
    try {
        if (configPath.isEmpty())
            objectNet = cv::dnn::readNet(modelPath.toStdString());
        else
            objectNet = cv::dnn::readNet(modelPath.toStdString(), configPath.toStdString());
        if (!objectNet.empty())
            setDnnBackend(objectNet);
        return !objectNet.empty();
    } catch (const cv::Exception& ex) {
        qWarning() << "Failed to load object model:" << ex.what();
        return false;
    }
}

void AIProcessor::setFaceConfidence(float threshold)
{
    faceThreshold = std::clamp(threshold, 0.05f, 0.99f);
}

void AIProcessor::setObjectConfidence(float threshold)
{
    objectThreshold = std::clamp(threshold, 0.05f, 0.99f);
}

void AIProcessor::setRecognitionIntervalMs(int interval)
{
    const int newInterval = std::max(-1, interval);
    if (recognitionIntervalMs == newInterval)
        return;
    recognitionIntervalMs = newInterval;
    recognitionCacheTtlMs = recognitionIntervalMs > 0
        ? std::max(recognitionIntervalMs, 100)
        : 300;
    if (!isRecognitionRateLimited() && recognitionTimer.isValid())
        recognitionTimer.invalidate();
    emit recognitionIntervalChanged(recognitionIntervalMs);
}

void AIProcessor::resetBackground()
{
}

ProcessedFrame AIProcessor::processFrame(const cv::Mat& frame, int cameraId)
{
    ProcessedFrame result;
    if (frame.empty())
        return result;

    cv::Mat canvas;
    frame.copyTo(canvas);

    QVector<Detection> detections;
    detections += detectFaces(frame, canvas, cameraId);
    detections += detectPersons(frame, canvas);
    detections += detectObjects(frame, canvas);

    result.annotated = canvas;
    result.detections = detections;

    if (!detections.isEmpty())
        emit detectionsReady(detections);

    return result;
}

bool AIProcessor::loadEmbedModel(const QString& modelPath)
{
    if (!embedEngine)
        embedEngine = std::make_unique<AIProcessorONNX>();
    const bool loaded = embedEngine->loadModel(modelPath);
    if (!loaded)
        qWarning() << "Failed to load embedding model from" << modelPath;
    return loaded;
}

void AIProcessor::setPreferGpuForEmbeddings(bool enable)
{
    if (!embedEngine)
        embedEngine = std::make_unique<AIProcessorONNX>();
    if (embedEngine->prefersOpenVino() == enable)
        return;
    embedEngine->setPreferOpenVino(enable);
    emit embeddingBackendChanged(enable);
}

bool AIProcessor::prefersGpuForEmbeddings() const
{
    return embedEngine ? embedEngine->prefersOpenVino() : true;
}

bool AIProcessor::hasEmbedModel() const
{
    return embedEngine && embedEngine->isLoaded();
}

bool AIProcessor::loadKnownEmbeddings(const QString& jsonPath)
{
    embeddingsPath = resolvePath(jsonPath);
    QFile file(embeddingsPath);
    if (!file.exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open embeddings file:" << embeddingsPath << file.errorString();
        return false;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        qWarning() << "Embeddings file is not an array:" << embeddingsPath;
        return false;
    }

    QVector<LabeledEmbedding> items;
    for (const auto& value : doc.array()) {
        if (!value.isObject())
            continue;
        const auto obj = value.toObject();
        const QString id = obj.value(QStringLiteral("id")).toString();
        const QString name = obj.value(QStringLiteral("name")).toString();
        const auto embArray = obj.value(QStringLiteral("embedding")).toArray();
        const QString imagePath = obj.value(QStringLiteral("image")).toString();
        const int samples = obj.value(QStringLiteral("samples")).toInt(1);
        if (name.isEmpty() || embArray.isEmpty())
            continue;

        std::vector<float> emb;
        emb.reserve(embArray.size());
        for (const auto& v : embArray) {
            emb.push_back(static_cast<float>(v.toDouble()));
        }
        LabeledEmbedding entry;
        entry.id = id;
        entry.name = name;
        entry.embedding = std::move(emb);
        entry.previewPath = imagePath;
        entry.sampleCount = std::max(1, samples);
        items.append(entry);
    }

    setKnownEmbeddings(items);
    invalidateRecognitionCache();
    emit faceDatabaseChanged();
    return !knownEmbeddings.empty();
}

void AIProcessor::setKnownEmbeddings(const QVector<LabeledEmbedding>& labeledEmbeddings)
{
    QMutexLocker locker(&knownEmbeddingsMutex);
    knownEmbeddings.clear();
    knownEmbeddings.reserve(labeledEmbeddings.size());
    for (const auto& pair : labeledEmbeddings) {
        LabeledEmbedding entry = pair;
        if (entry.id.isEmpty())
            entry.id = generateFaceId();
        entry.sampleCount = std::max(1, entry.sampleCount);
        normalizeEmbedding(entry.embedding);
        knownEmbeddings.push_back(std::move(entry));
    }
}

QString AIProcessor::saveFacePreview(const QString& name, const cv::Mat& faceBgr) const
{
    if (faceBgr.empty())
        return {};

    const QString safeBase = sanitizeName(name);
    const QString relDir = QStringLiteral("config/faces");
    const QString relPath = QStringLiteral("%1/%2.png").arg(relDir, safeBase);
    const QString absDir = resolvePath(relDir);
    QDir dir(absDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "Failed to create face preview directory at" << absDir;
        return {};
    }

    const QString absPath = absDir + "/" + safeBase + ".png";
    if (!cv::imwrite(absPath.toStdString(), faceBgr)) {
        qWarning() << "Failed to write face preview to" << absPath;
        return {};
    }
    return relPath;
}

QString AIProcessor::resolvePreviewPath(const QString& storedPath) const
{
    if (storedPath.isEmpty())
        return {};
    const QString absPath = resolvePath(storedPath);
    return QFileInfo::exists(absPath) ? absPath : QString();
}

QString AIProcessor::generateFaceId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void AIProcessor::invalidateRecognitionCache()
{
    QMutexLocker locker(&recognitionCacheMutex);
    recognitionCache.clear();
}

bool AIProcessor::removeFacePreviewFile(const QString& storedPath) const
{
    if (storedPath.isEmpty())
        return true;
    const QString absPath = resolvePath(storedPath);
    if (!QFileInfo::exists(absPath))
        return true;
    QFile file(absPath);
    if (!file.remove()) {
        qWarning() << "Failed to remove face preview at" << absPath << file.errorString();
        return false;
    }
    return true;
}

bool AIProcessor::addKnownEmbedding(const QString& name, const cv::Mat& faceBgr, const QString& savePath)
{
    if (!hasEmbedModel()) {
        qWarning() << "Embedding model not loaded; cannot add embedding for" << name;
        return false;
    }
    if (name.trimmed().isEmpty()) {
        qWarning() << "Embedding name is empty";
        return false;
    }
    const std::vector<float> emb = computeEmbedding(faceBgr);
    if (emb.empty()) {
        qWarning() << "Failed to compute embedding for" << name;
        return false;
    }

    LabeledEmbedding entry;
    entry.id = generateFaceId();
    entry.name = name.trimmed();
    entry.embedding = emb;
    entry.previewPath = saveFacePreview(entry.name, faceBgr);
    entry.sampleCount = 1;
    normalizeEmbedding(entry.embedding);
    {
        QMutexLocker locker(&knownEmbeddingsMutex);
        knownEmbeddings.push_back(entry);
    }

    const QString path = savePath.isEmpty() ? embeddingsPath : savePath;
    if (!persistKnownEmbeddings(path)) {
        qWarning() << "Failed to persist embeddings to" << path;
        return false;
    }
    invalidateRecognitionCache();
    emit faceDatabaseChanged();
    return true;
}

std::vector<float> AIProcessor::computeEmbedding(const cv::Mat& faceBgr) const
{
    if (!embedEngine)
        return {};
    return embedEngine->computeEmbedding(faceBgr);
}

QVector<AIProcessor::FaceProfile> AIProcessor::listFaceProfiles() const
{
    QVector<FaceProfile> profiles;
    QMutexLocker locker(&knownEmbeddingsMutex);
    profiles.reserve(static_cast<int>(knownEmbeddings.size()));
    for (const auto& entry : knownEmbeddings) {
        FaceProfile profile;
        profile.id = entry.id;
        profile.name = entry.name;
        profile.sampleCount = std::max(1, entry.sampleCount);
        profile.previewPath = resolvePreviewPath(entry.previewPath);
        profiles.append(profile);
    }
    return profiles;
}

bool AIProcessor::renameFaceProfile(const QString& id, const QString& newName)
{
    const QString trimmed = newName.trimmed();
    if (id.isEmpty() || trimmed.isEmpty())
        return false;

    {
        QMutexLocker locker(&knownEmbeddingsMutex);
        auto it = std::find_if(knownEmbeddings.begin(), knownEmbeddings.end(),
            [&](const LabeledEmbedding& entry) { return entry.id == id; });
        if (it == knownEmbeddings.end())
            return false;
        if (it->name == trimmed)
            return true;
        it->name = trimmed;
    }

    if (!persistKnownEmbeddings(embeddingsPath)) {
        loadKnownEmbeddings(embeddingsPath);
        return false;
    }
    invalidateRecognitionCache();
    emit faceDatabaseChanged();
    return true;
}

bool AIProcessor::deleteFaceProfile(const QString& id)
{
    if (id.isEmpty())
        return false;

    QString previewPath;
    bool allowPreviewRemoval = false;
    {
        QMutexLocker locker(&knownEmbeddingsMutex);
        auto it = std::find_if(knownEmbeddings.begin(), knownEmbeddings.end(),
            [&](const LabeledEmbedding& entry) { return entry.id == id; });
        if (it == knownEmbeddings.end())
            return false;
        previewPath = it->previewPath;
        knownEmbeddings.erase(it);
        if (!previewPath.isEmpty()) {
            allowPreviewRemoval = std::none_of(knownEmbeddings.begin(), knownEmbeddings.end(),
                [&](const LabeledEmbedding& entry) { return entry.previewPath == previewPath; });
        }
    }

    if (!persistKnownEmbeddings(embeddingsPath)) {
        loadKnownEmbeddings(embeddingsPath);
        return false;
    }
    if (allowPreviewRemoval)
        removeFacePreviewFile(previewPath);
    invalidateRecognitionCache();
    emit faceDatabaseChanged();
    return true;
}

bool AIProcessor::mergeFaceProfiles(const QString& targetId, const QStringList& duplicateIds)
{
    if (targetId.isEmpty() || duplicateIds.isEmpty())
        return false;

    QSet<QString> uniqueIds;
    for (const auto& id : duplicateIds) {
        if (id.isEmpty() || id == targetId)
            continue;
        uniqueIds.insert(id);
    }
    if (uniqueIds.isEmpty())
        return false;

    QVector<int> removalIndices;
    QStringList previewPathsToConsider;
    QStringList previewPathsToDelete;
    bool merged = false;

    {
        QMutexLocker locker(&knownEmbeddingsMutex);
        int targetIndex = -1;
        for (int i = 0; i < static_cast<int>(knownEmbeddings.size()); ++i) {
            if (knownEmbeddings[i].id == targetId) {
                targetIndex = i;
                break;
            }
        }
        if (targetIndex < 0)
            return false;

        LabeledEmbedding& target = knownEmbeddings[targetIndex];
        const size_t embSize = target.embedding.size();
        if (embSize == 0)
            return false;

        double totalSamples = std::max(1, target.sampleCount);
        std::vector<double> accumulator(embSize, 0.0);
        for (size_t i = 0; i < embSize; ++i)
            accumulator[i] = static_cast<double>(target.embedding[i]) * totalSamples;

        for (int i = 0; i < static_cast<int>(knownEmbeddings.size()); ++i) {
            const LabeledEmbedding& candidate = knownEmbeddings[i];
            if (!uniqueIds.contains(candidate.id))
                continue;
            if (candidate.embedding.size() != embSize)
                continue;

            const int samples = std::max(1, candidate.sampleCount);
            for (size_t j = 0; j < embSize; ++j)
                accumulator[j] += static_cast<double>(candidate.embedding[j]) * samples;
            totalSamples += samples;

            if (target.previewPath.isEmpty() && !candidate.previewPath.isEmpty()) {
                target.previewPath = candidate.previewPath;
            } else if (!candidate.previewPath.isEmpty() && candidate.previewPath != target.previewPath) {
                previewPathsToConsider.append(candidate.previewPath);
            }

            removalIndices.append(i);
            merged = true;
        }

        if (!merged)
            return false;

        target.sampleCount = static_cast<int>(totalSamples);
        for (size_t i = 0; i < embSize; ++i)
            target.embedding[i] = static_cast<float>(accumulator[i] / totalSamples);
        normalizeEmbedding(target.embedding);

        std::sort(removalIndices.begin(), removalIndices.end(),
            [](int a, int b) { return a > b; });
        for (int index : removalIndices) {
            if (index == targetIndex)
                continue;
            if (index < 0 || index >= static_cast<int>(knownEmbeddings.size()))
                continue;
            if (index < targetIndex)
                --targetIndex;
            knownEmbeddings.erase(knownEmbeddings.begin() + index);
        }

        for (const QString& path : previewPathsToConsider) {
            const bool stillUsed = std::any_of(knownEmbeddings.begin(), knownEmbeddings.end(),
                [&](const LabeledEmbedding& entry) { return entry.previewPath == path; });
            if (!stillUsed)
                previewPathsToDelete.append(path);
        }
    }

    if (!persistKnownEmbeddings(embeddingsPath)) {
        loadKnownEmbeddings(embeddingsPath);
        return false;
    }
    for (const QString& path : previewPathsToDelete)
        removeFacePreviewFile(path);
    invalidateRecognitionCache();
    emit faceDatabaseChanged();
    return true;
}

void AIProcessor::processFrameAsync(int cameraId, const QImage& frame)
{
    const QSize frameSize = frame.size();
    const cv::Mat mat = imageToMat(frame);
    if (mat.empty()) {
        emit frameProcessed(cameraId, frame, {}, frameSize);
        return;
    }

    ProcessedFrame processed = processFrame(mat, cameraId);
    QImage annotated = processed.annotated.empty() ? frame : matToImage(processed.annotated);
    emit frameProcessed(cameraId, annotated, processed.detections, frameSize);
}

bool AIProcessor::isRecognitionReady() const
{
    if (!hasEmbedModel())
        return false;
    if (!isRecognitionRateLimited())
        return true;
    if (!recognitionTimer.isValid())
        return true;
    return recognitionTimer.hasExpired(recognitionIntervalMs);
}

bool AIProcessor::claimRecognitionSlot()
{
    if (!hasEmbedModel())
        return false;
    if (!isRecognitionRateLimited())
        return true;
    if (!recognitionTimer.isValid()) {
        recognitionTimer.start();
        return true;
    }
    if (!recognitionTimer.hasExpired(recognitionIntervalMs))
        return false;
    recognitionTimer.restart();
    return true;
}

float AIProcessor::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const
{
    if (a.size() != b.size() || a.empty())
        return -1.0f;

    double dot = 0.0;
    double normA = 0.0;
    double normB = 0.0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += static_cast<double>(a[i]) * b[i];
        normA += static_cast<double>(a[i]) * a[i];
        normB += static_cast<double>(b[i]) * b[i];
    }
    if (normA <= 0.0 || normB <= 0.0)
        return -1.0f;
    return static_cast<float>(dot / (std::sqrt(normA) * std::sqrt(normB)));
}

void AIProcessor::normalizeEmbedding(std::vector<float>& embedding)
{
    double norm = 0.0;
    for (float v : embedding)
        norm += static_cast<double>(v) * v;
    if (norm <= 0.0)
        return;
    norm = std::sqrt(norm);
    for (float& v : embedding)
        v = static_cast<float>(v / norm);
}

bool AIProcessor::persistKnownEmbeddings(const QString& path) const
{
    const QString outPath = resolvePath(path);
    QDir().mkpath(QFileInfo(outPath).absolutePath());

    std::vector<LabeledEmbedding> snapshot;
    {
        QMutexLocker locker(&knownEmbeddingsMutex);
        snapshot = knownEmbeddings;
    }

    QJsonArray array;
    for (const auto& item : snapshot) {
        QJsonObject obj;
        if (!item.id.isEmpty())
            obj.insert(QStringLiteral("id"), item.id);
        obj.insert(QStringLiteral("name"), item.name);
        QJsonArray embArray;
        for (float v : item.embedding)
            embArray.append(static_cast<double>(v));
        obj.insert(QStringLiteral("embedding"), embArray);
        if (!item.previewPath.isEmpty())
            obj.insert(QStringLiteral("image"), item.previewPath);
        obj.insert(QStringLiteral("samples"), std::max(1, item.sampleCount));
        array.append(obj);
    }

    QFile file(outPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open embeddings file for write:" << outPath << file.errorString();
        return false;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return true;
}

QVector<Detection> AIProcessor::detectFaces(const cv::Mat& frame, cv::Mat& canvas, int cameraId)
{
    QVector<Detection> detections;
    if (faceNet.empty())
        return detections;

    const QSize frameSize(frame.cols, frame.rows);
    const ScaledFrame scaled = makeScaledFrame(frame, kMaxProcessWidth);
    const cv::Mat& input = scaled.image;
    const float invScale = scaled.scale > 0.0f ? 1.0f / scaled.scale : 1.0f;

    cv::Mat blob = cv::dnn::blobFromImage(input, 1.0, kDnnInputSize, kMeanValues, false, false);
    faceNet.setInput(blob);
    cv::Mat out = faceNet.forward();

    const int detectionsCount = out.size[2];
    for (int i = 0; i < detectionsCount; ++i) {
        const float confidence = out.ptr<float>(0, 0, i)[2];
        if (confidence < faceThreshold)
            continue;

        const float x1 = out.ptr<float>(0, 0, i)[3] * input.cols * invScale;
        const float y1 = out.ptr<float>(0, 0, i)[4] * input.rows * invScale;
        const float x2 = out.ptr<float>(0, 0, i)[5] * input.cols * invScale;
        const float y2 = out.ptr<float>(0, 0, i)[6] * input.rows * invScale;

        cv::Rect rect(cv::Point(static_cast<int>(x1), static_cast<int>(y1)),
            cv::Point(static_cast<int>(x2), static_cast<int>(y2)));
        rect &= cv::Rect(0, 0, frame.cols, frame.rows);
        if (rect.empty())
            continue;

        Detection detection;
        detection.label = QStringLiteral("Face");
        detection.category = QStringLiteral("Face");
        detection.confidence = confidence;
        detection.rect = toRect(rect, frame.size());
        detection.color = faceColor;
        QString textLabel = QString("%1 %2%").arg(detection.label).arg(static_cast<int>(confidence * 100));

        const QString cacheKey = detectionCacheKey(cameraId, detection.rect, frameSize);
        RecognitionCacheEntry cacheEntry;
        if (tryGetCachedRecognition(cacheKey, cacheEntry)) {
            if (!cacheEntry.label.isEmpty()) {
                detection.label = cacheEntry.label;
                detection.category = QStringLiteral("Face");
                detection.color = cacheEntry.color;
                detection.previewPath = resolvePreviewPath(cacheEntry.previewPath);
                textLabel = cacheEntry.similarity >= 0.0f
                    ? QString("%1 (%2)").arg(cacheEntry.label, QString::number(cacheEntry.similarity, 'f', 2))
                    : cacheEntry.label;
            }
        } else if (hasEmbedModel()) {
            const bool allowRecognition = !isRecognitionRateLimited() || isRecognitionReady();
            if (allowRecognition) {
                const cv::Mat faceCopy = frame(rect).clone();
                scheduleEmbeddingJob(cacheKey, faceCopy);
                textLabel = QStringLiteral("Face (processing)");
            } else {
                textLabel = QStringLiteral("Face (waiting)");
            }
        }
        detections.append(detection);

        cv::rectangle(canvas, rect, toScalar(detection.color), 2);
        const int textY = std::min(rect.y + rect.height + 18, frame.rows - 4);
        cv::putText(canvas, textLabel.toStdString(), cv::Point(rect.x, textY),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, toScalar(detection.color), 1, cv::LINE_AA);
    }

    return detections;
}

QVector<Detection> AIProcessor::detectPersons(const cv::Mat& frame, cv::Mat& canvas)
{
    QVector<Detection> detections;

    const ScaledFrame scaled = makeScaledFrame(frame, kMaxProcessWidth);
    const cv::Mat& input = scaled.image;
    const float invScale = scaled.scale > 0.0f ? 1.0f / scaled.scale : 1.0f;

    std::vector<cv::Rect> found;
    std::vector<double> weights;
    personHog.detectMultiScale(input, found, weights, 0, cv::Size(8, 8), cv::Size(), 1.05, 2.0, false);

    for (size_t i = 0; i < found.size(); ++i) {
        cv::Rect rect = found[i];
        if (scaled.scale != 1.0f) {
            rect.x = static_cast<int>(rect.x * invScale);
            rect.y = static_cast<int>(rect.y * invScale);
            rect.width = static_cast<int>(rect.width * invScale);
            rect.height = static_cast<int>(rect.height * invScale);
        }
        rect &= cv::Rect(0, 0, frame.cols, frame.rows);
        if (rect.empty())
            continue;

        Detection detection;
        detection.label = QStringLiteral("Person");
        detection.category = QStringLiteral("Person");
        detection.confidence = weights.size() == found.size() ? static_cast<float>(weights[i]) : 1.0f;
        detection.rect = toRect(rect, frame.size());
        detection.color = personColor;
        detections.append(detection);

        cv::rectangle(canvas, rect, toScalar(personColor), 2);
    }

    return detections;
}

QVector<Detection> AIProcessor::detectObjects(const cv::Mat& frame, cv::Mat& canvas)
{
    QVector<Detection> detections;
    if (objectNet.empty())
        return detections;

    const ScaledFrame scaled = makeScaledFrame(frame, kMaxProcessWidth);
    const cv::Mat& input = scaled.image;
    const float invScale = scaled.scale > 0.0f ? 1.0f / scaled.scale : 1.0f;

    cv::Mat blob = cv::dnn::blobFromImage(input, 1.0, kDnnInputSize, cv::Scalar(), true, false);
    objectNet.setInput(blob);
    cv::Mat out = objectNet.forward();

    if (out.dims == 0)
        return detections;

    const int rows = out.size[2];
    for (int i = 0; i < rows; ++i) {
        const float confidence = out.ptr<float>(0, 0, i)[2];
        if (confidence < objectThreshold)
            continue;

        const float x1 = out.ptr<float>(0, 0, i)[3] * input.cols * invScale;
        const float y1 = out.ptr<float>(0, 0, i)[4] * input.rows * invScale;
        const float x2 = out.ptr<float>(0, 0, i)[5] * input.cols * invScale;
        const float y2 = out.ptr<float>(0, 0, i)[6] * input.rows * invScale;

        cv::Rect rect(cv::Point(static_cast<int>(x1), static_cast<int>(y1)),
            cv::Point(static_cast<int>(x2), static_cast<int>(y2)));
        rect &= cv::Rect(0, 0, frame.cols, frame.rows);
        if (rect.empty())
            continue;

        Detection detection;
        detection.label = QStringLiteral("Object");
        detection.category = QStringLiteral("Object");
        detection.confidence = confidence;
        detection.rect = toRect(rect, frame.size());
        detection.color = objectColor;
        detections.append(detection);

        cv::rectangle(canvas, rect, toScalar(objectColor), 2);
        const QString text = QString("%1 %2%").arg(detection.label).arg(static_cast<int>(confidence * 100));
        cv::putText(canvas, text.toStdString(), rect.tl() + cv::Point(0, -4),
            cv::FONT_HERSHEY_SIMPLEX, 0.5, toScalar(objectColor), 1, cv::LINE_AA);
    }

    return detections;
}

cv::Scalar AIProcessor::toScalar(const QColor& color)
{
    return cv::Scalar(color.blue(), color.green(), color.red());
}

QRect AIProcessor::toRect(const cv::Rect& rect, const cv::Size& bounds)
{
    const int x = std::clamp(rect.x, 0, bounds.width);
    const int y = std::clamp(rect.y, 0, bounds.height);
    const int w = std::clamp(rect.width, 0, bounds.width - x);
    const int h = std::clamp(rect.height, 0, bounds.height - y);
    return QRect(x, y, w, h);
}

cv::Mat AIProcessor::imageToMat(const QImage& image)
{
    if (image.isNull())
        return {};
    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(converted.height(), converted.width(), CV_8UC3,
        const_cast<uchar*>(converted.bits()), converted.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

QImage AIProcessor::matToImage(const cv::Mat& mat)
{
    if (mat.empty())
        return {};
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}

QString AIProcessor::detectionCacheKey(int cameraId, const QRect& rect, const QSize& bounds) const
{
    if (rect.isEmpty() || bounds.isEmpty())
        return {};

    const int bucketBase = std::min(bounds.width(), bounds.height()) / 32;
    const int bucket = std::max(8, bucketBase);
    const QPoint center = rect.center();
    const int cx = center.x() / bucket;
    const int cy = center.y() / bucket;
    const int w = std::max(1, rect.width() / bucket);
    const int h = std::max(1, rect.height() / bucket);
    return QStringLiteral("%1_%2_%3_%4_%5")
        .arg(cameraId)
        .arg(cx)
        .arg(cy)
        .arg(w)
        .arg(h);
}

bool AIProcessor::tryGetCachedRecognition(const QString& key, RecognitionCacheEntry& entry) const
{
    QMutexLocker locker(&recognitionCacheMutex);
    const auto it = recognitionCache.constFind(key);
    if (it == recognitionCache.constEnd())
        return false;
    if (!it->hasResult)
        return false;
    if (!it->timer.isValid() || it->timer.elapsed() > recognitionCacheTtlMs)
        return false;
    entry = *it;
    return true;
}

void AIProcessor::scheduleEmbeddingJob(const QString& key, cv::Mat face)
{
    if (face.empty() || !hasEmbedModel())
        return;

    {
        QMutexLocker locker(&recognitionCacheMutex);
        auto& entry = recognitionCache[key];
        if (entry.pending)
            return;
        if (entry.timer.isValid() && entry.timer.elapsed() < recognitionCacheTtlMs)
            return;
        entry.pending = true;
    }

    if (isRecognitionRateLimited()) {
        if (!recognitionTimer.isValid())
            recognitionTimer.start();
        else
            recognitionTimer.restart();
    }

    QtConcurrent::run([this, key, face = std::move(face)]() {
        RecognitionCacheEntry result = runRecognitionTask(face);
        QMetaObject::invokeMethod(this, [this, key, result]() {
            completeRecognitionJob(key, result);
        }, Qt::QueuedConnection);
    });
}

AIProcessor::RecognitionCacheEntry AIProcessor::runRecognitionTask(const cv::Mat& face) const
{
    RecognitionCacheEntry entry;
    entry.color = faceColor;
    entry.hasResult = true;
    if (face.empty())
        return entry;

    const std::vector<float> probe = computeEmbedding(face);
    if (probe.empty())
        return entry;

    float bestSim = -1.0f;
    QString bestName;
    QString previewPath;
    {
        QMutexLocker locker(&knownEmbeddingsMutex);
        for (const auto& known : knownEmbeddings) {
            const float sim = cosineSimilarity(probe, known.embedding);
            if (sim > bestSim) {
                bestSim = sim;
                bestName = known.name;
                previewPath = known.previewPath;
            }
        }
    }

    if (bestSim >= recognitionThreshold && !bestName.isEmpty()) {
        entry.label = bestName;
        entry.color = recognizedFaceColor;
        entry.similarity = bestSim;
        entry.previewPath = previewPath;
    }
    return entry;
}

void AIProcessor::completeRecognitionJob(const QString& key, const RecognitionCacheEntry& entry)
{
    QMutexLocker locker(&recognitionCacheMutex);
    auto& cacheEntry = recognitionCache[key];
    cacheEntry = entry;
    cacheEntry.pending = false;
    cacheEntry.hasResult = true;
    cacheEntry.timer.restart();
}
