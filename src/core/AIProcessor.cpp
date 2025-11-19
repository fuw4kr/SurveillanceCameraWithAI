#include "AIProcessor.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/core/cuda.hpp>
#include <onnxruntime_c_api.h>
#ifdef _WIN32
#include <Windows.h>
#undef min
#undef max
#endif
#include <cmath>
#include <string>
#include <thread>

namespace {
const cv::Size kDnnInputSize(300, 300);
const cv::Scalar kMeanValues(104.0, 177.0, 123.0); // commonly used for FaceNet style models
const QString kDefaultEmbeddingsPath = QStringLiteral("config/embeddings.json");
constexpr int kMaxProcessWidth = 960; // downscale wide frames for faster inference while keeping quality

Ort::Env& ortEnv()
{
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "ai_processor");
    return env;
}

void setPreferredDmlFeatureLevel()
{
#ifdef _WIN32
    // Ask DML EP to cap graphics feature level to 12_0 to avoid 12_1/12_2 negotiations on unstable drivers.
    qputenv("ORT_DML_PREFERRED_FEATURE_LEVEL", "12_0");
    qputenv("ORT_DML_REQUIRED_FEATURE_LEVEL", "12_0");
    // Set extra aliases in case the runtime honors alternative names.
    qputenv("DML_PREFERRED_FEATURE_LEVEL", "12_0");
    qputenv("DML_REQUIRED_FEATURE_LEVEL", "12_0");
    qputenv("ORT_DML_MINIMUM_FEATURE_LEVEL", "12_0");
#endif
}

// Ensure feature-level hints are set before any ORT initialization happens.
const bool kInitDmlFeatureEnv = []() {
    setPreferredDmlFeatureLevel();
    return true;
}();

bool shouldUseDirectML()
{
#ifdef _WIN32
    if (qEnvironmentVariableIsSet("AIP_DISABLE_DML")) {
        qInfo() << "DirectML disabled via AIP_DISABLE_DML";
        return false;
    }
    // Default: try GPU unless explicitly disabled.
    return true;
#else
    return false;
#endif
}

bool tryAppendDml(Ort::SessionOptions& options)
{
#ifdef _WIN32
    setPreferredDmlFeatureLevel();

    // Resolve DirectML EP append symbol dynamically; works only if onnxruntime.dll exports it.
    HMODULE ortLib = GetModuleHandleW(L"onnxruntime.dll");
    if (!ortLib) {
        qWarning() << "ONNX Runtime: onnxruntime.dll not loaded; skipping DirectML";
        return false;
    }

    using FnAppendDml = OrtStatus* (ORT_API_CALL*)(OrtSessionOptions* options, uint32_t device_id);
    const auto fn = reinterpret_cast<FnAppendDml>(GetProcAddress(ortLib, "OrtSessionOptionsAppendExecutionProvider_DML"));
    if (!fn) {
        qInfo() << "ONNX Runtime: DirectML EP symbol not found; CPU will be used";
        return false;
    }

    QString lastError;
    constexpr uint32_t kMaxAdapters = 4;
    for (uint32_t adapterId = 0; adapterId < kMaxAdapters; ++adapterId) {
        if (OrtStatus* status = fn(options, adapterId)) {
            const char* msg = Ort::GetApi().GetErrorMessage(status);
            lastError = QString::fromUtf8(msg ? msg : "");
            Ort::GetApi().ReleaseStatus(status);
            continue;
        }
        qInfo() << "ONNX Runtime: DirectML execution provider enabled on adapter" << adapterId;
        return true;
    }

    qWarning() << "ONNX Runtime: DirectML append failed on adapters 0-" << (kMaxAdapters - 1)
               << "; CPU will be used:" << lastError;
    return false;
#else
    Q_UNUSED(options);
    qInfo() << "ONNX Runtime: DirectML not available on this platform";
    return false;
#endif
}

void configureEmbedOptions(Ort::SessionOptions& options, bool enableDml, bool& dmlEnabled)
{
    options = Ort::SessionOptions();
    options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    // Keep CPU pressure low (temporary mitigation when GPU initialization fails and CPU is used).
    options.SetIntraOpNumThreads(2);
    options.SetInterOpNumThreads(1);
    dmlEnabled = enableDml && tryAppendDml(options);
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
    const bool preferDml = shouldUseDirectML();
    configureEmbedOptions(embedOptions, preferDml, embedUsesDirectML);
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

void AIProcessor::resetBackground()
{
}

ProcessedFrame AIProcessor::processFrame(const cv::Mat& frame)
{
    ProcessedFrame result;
    if (frame.empty())
        return result;

    cv::Mat canvas;
    frame.copyTo(canvas);

    QVector<Detection> detections;
    detections += detectFaces(frame, canvas);
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
    auto loadWithOptions = [&](const Ort::SessionOptions& opts) -> bool {
        const std::wstring wModelPath = modelPath.toStdWString();
        embedSession.reset(new Ort::Session(ortEnv(), wModelPath.c_str(), opts));
        return validateEmbedModel(*embedSession);
    };

    try {
        // ORT expects wide-char paths on Windows
        embedModelLoaded = loadWithOptions(embedOptions);
        if (embedModelLoaded)
            return true;
        if (embedUsesDirectML)
            qWarning() << "Embedding model validation failed on DirectML; trying CPU fallback";
    } catch (const Ort::Exception& ex) {
        qWarning() << "Failed to load embedding model:" << ex.what();
        embedModelLoaded = false;
        embedSession.reset();
    }

    if (embedUsesDirectML) {
        bool dmlEnabled = false;
        configureEmbedOptions(embedOptions, false, dmlEnabled);
        embedUsesDirectML = dmlEnabled;
        try {
            embedModelLoaded = loadWithOptions(embedOptions);
            return embedModelLoaded;
        } catch (const Ort::Exception& ex) {
            qWarning() << "CPU fallback for embedding model failed:" << ex.what();
            embedModelLoaded = false;
            embedSession.reset();
        }
    }

    return embedModelLoaded;
}

bool AIProcessor::validateEmbedModel(const Ort::Session& session)
{
    Ort::AllocatorWithDefaultOptions alloc;
    Ort::TypeInfo inputInfo = session.GetInputTypeInfo(0);
    auto tensorInfo = inputInfo.GetTensorTypeAndShapeInfo();
    embedInputShape = tensorInfo.GetShape();
    embedRunShape = embedInputShape;

    // Resolve dynamic dimensions (-1/0) to 1 so we can submit a concrete tensor shape.
    for (auto& dim : embedRunShape) {
        if (dim <= 0)
            dim = 1;
    }

    if (embedInputShape.size() == 4) {
        // NCHW
        embedChannels = static_cast<int>(embedInputShape[1]);
        embedHeight = static_cast<int>(embedInputShape[2]);
        embedWidth = static_cast<int>(embedInputShape[3]);
    } else if (embedInputShape.size() == 3) {
        // HWC
        embedHeight = static_cast<int>(embedInputShape[0]);
        embedWidth = static_cast<int>(embedInputShape[1]);
        embedChannels = static_cast<int>(embedInputShape[2]);
    } else {
        qWarning() << "Unexpected embedding input shape";
        return false;
    }

    embedTensorSize = 1;
    for (auto dim : embedRunShape) {
        if (dim > 0)
            embedTensorSize *= static_cast<size_t>(dim);
    }

    embedInputName = session.GetInputNameAllocated(0, alloc).get();
    embedOutputName = session.GetOutputNameAllocated(0, alloc).get();
    return true;
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
        const QString name = obj.value(QStringLiteral("name")).toString();
        const auto embArray = obj.value(QStringLiteral("embedding")).toArray();
        const QString imagePath = obj.value(QStringLiteral("image")).toString();
        if (name.isEmpty() || embArray.isEmpty())
            continue;

        std::vector<float> emb;
        emb.reserve(embArray.size());
        for (const auto& v : embArray) {
            emb.push_back(static_cast<float>(v.toDouble()));
        }
        LabeledEmbedding entry;
        entry.name = name;
        entry.embedding = std::move(emb);
        entry.previewPath = imagePath;
        items.append(entry);
    }

    setKnownEmbeddings(items);
    return !knownEmbeddings.empty();
}

void AIProcessor::setKnownEmbeddings(const QVector<LabeledEmbedding>& labeledEmbeddings)
{
    knownEmbeddings.clear();
    knownEmbeddings.reserve(labeledEmbeddings.size());
    for (const auto& pair : labeledEmbeddings) {
        LabeledEmbedding entry{ pair.name, pair.embedding, pair.previewPath };
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

bool AIProcessor::addKnownEmbedding(const QString& name, const cv::Mat& faceBgr, const QString& savePath)
{
    if (!embedModelLoaded || !embedSession) {
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
    entry.name = name.trimmed();
    entry.embedding = emb;
    entry.previewPath = saveFacePreview(entry.name, faceBgr);
    normalizeEmbedding(entry.embedding);
    knownEmbeddings.push_back(entry);

    const QString path = savePath.isEmpty() ? embeddingsPath : savePath;
    if (!persistKnownEmbeddings(path)) {
        qWarning() << "Failed to persist embeddings to" << path;
        return false;
    }
    return true;
}

bool AIProcessor::preprocessFace(const cv::Mat& face, std::vector<float>& tensor) const
{
    if (face.empty() || embedTensorSize == 0)
        return false;

    cv::Mat rgb;
    cv::cvtColor(face, rgb, cv::COLOR_BGR2RGB);
    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(embedWidth, embedHeight), 0, 0, cv::INTER_AREA);

    tensor.resize(embedTensorSize);
    const size_t channelStride = static_cast<size_t>(embedHeight) * embedWidth;
    for (int y = 0; y < embedHeight; ++y) {
        const uchar* row = resized.ptr<uchar>(y);
        for (int x = 0; x < embedWidth; ++x) {
            for (int c = 0; c < embedChannels; ++c) {
                const float v = (static_cast<float>(row[x * 3 + c]) - 127.5f) / 128.0f;
                tensor[c * channelStride + static_cast<size_t>(y) * embedWidth + x] = v;
            }
        }
    }
    return true;
}

std::vector<float> AIProcessor::computeEmbedding(const cv::Mat& faceBgr) const
{
    if (!embedModelLoaded || !embedSession)
        return {};

    if (!preprocessFace(faceBgr, embedInputBuffer))
        return {};

    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtDeviceAllocator, OrtMemTypeDefault);
    std::array<const char*, 1> inputNames{ embedInputName.c_str() };
    std::array<const char*, 1> outputNames{ embedOutputName.c_str() };

    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memInfo, embedInputBuffer.data(), embedInputBuffer.size(),
        embedRunShape.data(), embedRunShape.size());

    auto output = embedSession->Run(Ort::RunOptions{ nullptr }, inputNames.data(), &inputTensor, 1, outputNames.data(), 1);
    if (output.empty() || !output[0].IsTensor())
        return {};

    float* outData = output[0].GetTensorMutableData<float>();
    auto outShape = output[0].GetTensorTypeAndShapeInfo().GetShape();
    size_t outSize = 1;
    for (auto dim : outShape) {
        if (dim > 0)
            outSize *= static_cast<size_t>(dim);
    }

    std::vector<float> embedding(outData, outData + outSize);
    float norm = 0.0f;
    for (float v : embedding) norm += v * v;
    norm = std::sqrt(norm) + 1e-6f;
    for (float& v : embedding) v /= norm;
    return embedding;
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

    QJsonArray array;
    for (const auto& item : knownEmbeddings) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), item.name);
        QJsonArray embArray;
        for (float v : item.embedding)
            embArray.append(static_cast<double>(v));
        obj.insert(QStringLiteral("embedding"), embArray);
        if (!item.previewPath.isEmpty())
            obj.insert(QStringLiteral("image"), item.previewPath);
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

QVector<Detection> AIProcessor::detectFaces(const cv::Mat& frame, cv::Mat& canvas)
{
    QVector<Detection> detections;
    if (faceNet.empty())
        return detections;

    const ScaledFrame scaled = makeScaledFrame(frame, kMaxProcessWidth);
    const cv::Mat& input = scaled.image;
    const float invScale = scaled.scale > 0.0f ? 1.0f / scaled.scale : 1.0f;

    cv::Mat blob = cv::dnn::blobFromImage(input, 1.0, kDnnInputSize, kMeanValues, false, false);
    faceNet.setInput(blob);
    cv::Mat out = faceNet.forward();

    const auto findPreviewPath = [&](const QString& name) -> QString {
        if (name.isEmpty())
            return {};
        for (const auto& known : knownEmbeddings) {
            if (known.name.compare(name, Qt::CaseSensitive) == 0)
                return resolvePreviewPath(known.previewPath);
        }
        return {};
    };

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

        if (embedModelLoaded) {
            const cv::Mat faceRoi = frame(rect).clone();
            const std::vector<float> probe = computeEmbedding(faceRoi);
            if (!probe.empty()) {
                float bestSim = -1.0f;
                QString bestName;
                for (const auto& known : knownEmbeddings) {
                    const float sim = cosineSimilarity(probe, known.embedding);
                    if (sim > bestSim) {
                        bestSim = sim;
                        bestName = known.name;
                    }
                }
                if (bestSim >= recognitionThreshold && !bestName.isEmpty()) {
                    detection.label = bestName;
                    detection.category = QStringLiteral("Face");
                    detection.color = recognizedFaceColor;
                    detection.previewPath = findPreviewPath(bestName);
                    textLabel = QString("%1 (%2)").arg(bestName, QString::number(bestSim, 'f', 2));
                } else if (autoEnrollEnabled && autoEnrollTimer.hasExpired(autoEnrollCooldownMs)) {
                    // Auto-enroll unknown face
                    const QString newName = QStringLiteral("Person_%1").arg(knownEmbeddings.size() + 1);
                    if (addKnownEmbedding(newName, faceRoi)) {
                        detection.label = newName;
                        detection.category = QStringLiteral("Face");
                        detection.color = recognizedFaceColor;
                        detection.previewPath = findPreviewPath(newName);
                        textLabel = QString("%1 (enrolled)").arg(newName);
                        autoEnrollTimer.restart();
                    } else {
                        qWarning() << "Auto-enroll failed for face crop; embeddings not updated";
                    }
                }
            } else {
                qWarning() << "Embedding computation returned empty vector; model may not be loaded or input invalid";
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
