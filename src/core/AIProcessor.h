#ifndef AIPROCESSOR_H
#define AIPROCESSOR_H

/**
 * @file AIProcessor.h
 * @brief Core analytics engine for face/object/person detection and recognition.
 *
 * Wraps OpenCV DNN detectors and an ONNX-based embedding engine, exposing both
 * synchronous and asynchronous processing APIs. Maintains known embeddings,
 * performs rate-limited recognition, and emits Qt signals for UI consumers.
 *
 * @example
 * AIProcessor ai;
 * ai.loadFaceModel("assets/models/scrfd.onnx");
 * ai.loadEmbedModel("assets/models/arcface.onnx");
 * auto frame = ai.processFrame(mat, 0);
 */
#include "AIProcessorONNX.h"

#include <QObject>
#include <QVector>
#include <QRect>
#include <QString>
#include <QColor>
#include <QElapsedTimer>
#include <QMetaType>
#include <QImage>
#include <QSize>
#include <QMutex>
#include <QHash>
#include <QStringList>
#include <QSet>

#include <memory>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>

class AIProcessorONNX;

/**
 * @brief Detection metadata emitted by AIProcessor.
 */
struct Detection
{
    QRect rect;
    QString label;
    QString category; // e.g. Face/Object/Person for stats
    float confidence = 0.0f;
    QColor color = QColor(255, 255, 255);
    QString previewPath; // optional path to enrolled face crop
};

/**
 * @brief Result bundle containing annotated frame and detections.
 */
struct ProcessedFrame
{
    cv::Mat annotated;
    QVector<Detection> detections;
};

Q_DECLARE_METATYPE(Detection)
Q_DECLARE_METATYPE(QVector<Detection>)

/**
 * @brief Runs detection/recognition pipelines and emits results to consumers.
 *
 * Supports loading detection/object models, embedding models, and maintaining a
 * face database for recognition with caching and rate limiting.
 */
class AIProcessor : public QObject
{
    Q_OBJECT

public:
    explicit AIProcessor(QObject* parent = nullptr);

    struct FaceProfile {
        QString id;
        QString name;
        QString previewPath; // resolved absolute path if available
        int sampleCount = 1;
    };

    /**
     * @brief Loads a face detection model (e.g., YuNet or SSD).
     * @param modelPath Path to the model file.
     * @param configPath Optional configuration (for Caffe SSD).
     * @return bool True when loading succeeds.
     * @throws None
     * @example loadFaceModel("det.onnx", "deploy.prototxt");
     */
    bool loadFaceModel(const QString& modelPath, const QString& configPath = QString());
    /**
     * @brief Loads an object detection model.
     * @param modelPath Path to model weights.
     * @param configPath Optional config file.
     * @return bool True on success.
     * @throws None
     * @example loadObjectModel("yolov3.onnx");
     */
    bool loadObjectModel(const QString& modelPath, const QString& configPath = QString());
    /**
     * @brief Milliseconds between recognition passes (rate limiting).
     * @return int Interval in ms.
     * @throws None
     */
    int recognitionInterval() const { return recognitionIntervalMs; }
    /**
     * @brief Clears cached embeddings and resets timers.
     * @return void
     * @throws None
     */
    void resetBackground();
    /**
     * @brief Loads known embeddings from a JSON file.
     * @param jsonPath Path to embeddings file.
     * @return bool True when loaded successfully.
     * @throws None
     * @example loadKnownEmbeddings("config/embeddings.json");
     */
    bool loadKnownEmbeddings(const QString& jsonPath); // e.g. config/embeddings.json
    /**
     * @brief Adds a new labeled embedding and optionally persists it.
     * @param name Label/name.
     * @param faceBgr Face image in BGR.
     * @param savePath Optional path to persist embeddings.
     * @return bool True on success.
     * @throws None
     * @example addKnownEmbedding("Alice", faceMat, "config/embeddings.json");
     */
    bool addKnownEmbedding(const QString& name, const cv::Mat& faceBgr, const QString& savePath = QString());

    /**
     * @brief Indicates GPU preference for embeddings.
     * @return bool True if GPU backend is preferred.
     * @throws None
     */
    bool prefersGpuForEmbeddings() const;
    /**
     * @brief Loads an embedding model (ArcFace/MobileFaceNet/SFace/FaceNet).
     * @param modelPath Path to ONNX model.
     * @return bool True when loaded.
     * @throws None
     * @example loadEmbedModel("arcface.onnx");
     */
    bool loadEmbedModel(const QString& modelPath);
    /**
     * @brief Checks whether an embedding model is currently loaded.
     * @return bool True if embed model available.
     * @throws None
     */
    bool hasEmbedModel() const;
    /**
     * @brief Computes an embedding vector for a face crop.
     * @param faceBgr Face crop in BGR.
     * @return std::vector<float> Embedding vector (empty on failure).
     * @throws None
     * @example auto embedding = computeEmbedding(faceMat);
     */
    std::vector<float> computeEmbedding(const cv::Mat& faceBgr) const;

    /**
     * @brief Runs detection/recognition synchronously on a frame.
     * @param frame Input frame (BGR).
     * @param cameraId Optional camera id for tracking.
     * @return ProcessedFrame Annotated frame and detections.
     * @throws None
     * @example auto pf = processFrame(mat, 0);
     */
    ProcessedFrame processFrame(const cv::Mat& frame, int cameraId = -1);
    /**
     * @brief Lists known face profiles with metadata.
     * @return QVector<FaceProfile> Current face profiles.
     * @throws None
     * @example auto profiles = listFaceProfiles();
     */
    Q_INVOKABLE QVector<FaceProfile> listFaceProfiles() const;
    /**
     * @brief Renames a face profile by id.
     * @param id Profile id.
     * @param newName Desired name.
     * @return bool True if update succeeded.
     * @throws None
     * @example renameFaceProfile(profile.id, "New Name");
     */
    Q_INVOKABLE bool renameFaceProfile(const QString& id, const QString& newName);
    /**
     * @brief Deletes a face profile and associated preview.
     * @param id Profile id.
     * @return bool True on success.
     * @throws None
     * @example deleteFaceProfile(profile.id);
     */
    Q_INVOKABLE bool deleteFaceProfile(const QString& id);
    /**
     * @brief Merges duplicate profiles into a target profile.
     * @param targetId Destination profile id.
     * @param duplicateIds IDs to merge.
     * @return bool True if merge completed.
     * @throws None
     * @example mergeFaceProfiles(targetId, {"dup1", "dup2"});
     */
    Q_INVOKABLE bool mergeFaceProfiles(const QString& targetId, const QStringList& duplicateIds);

public slots:
    /** @brief Sets detection confidence threshold for faces. */
    void setFaceConfidence(float threshold);
    /** @brief Sets detection confidence threshold for objects. */
    void setObjectConfidence(float threshold);
    /** @brief Adjusts recognition rate-limit interval in milliseconds. */
    void setRecognitionIntervalMs(int intervalMs);
    /** @brief Sets GPU preference for embedding backend. */
    void setPreferGpuForEmbeddings(bool enable);
    /** @brief Processes a QImage frame asynchronously. */
    void processFrameAsync(int cameraId, const QImage& frame);
    /** @brief Async convenience to load embed model on worker thread. */
    void loadEmbedModelAsync(const QString& modelPath) { loadEmbedModel(modelPath); }
    /** @brief Async convenience to load face detector. */
    void loadFaceModelAsync(const QString& modelPath, const QString& configPath = QString()) { loadFaceModel(modelPath, configPath); }
    /** @brief Async convenience to load object detector. */
    void loadObjectModelAsync(const QString& modelPath, const QString& configPath = QString()) { loadObjectModel(modelPath, configPath); }

signals:
    void detectionsReady(const QVector<Detection>& detections);
    void frameProcessed(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);
    void recognitionIntervalChanged(int intervalMs);
    void embeddingBackendChanged(bool preferGpu);
    void faceDatabaseChanged();
    void faceAutoEnrolled(const QString& label, const QVector<float>& embedding, const QImage& preview);

private:
    struct LabeledEmbedding {
        QString id;
        QString name;
        std::vector<float> embedding;
        QString previewPath;
        int sampleCount = 1;
    };

    struct RecognitionCacheEntry {
        QString label;
        QString previewPath;
        QColor color = QColor(79, 70, 229);
        float similarity = -1.0f;
        QElapsedTimer timer;
        bool pending = false;
        bool hasResult = false;
    };

    struct ObjectProposal {
        cv::Rect rect;
        float confidence = 0.0f;
        int classId = -1;
    };

    enum class FaceDetectorMode {
        None,
        Ssd,
        YuNet
    };

    QVector<Detection> detectFaces(const cv::Mat& frame, cv::Mat& canvas, int cameraId);
    QVector<Detection> detectObjects(const cv::Mat& frame, cv::Mat& canvas, const std::vector<ObjectProposal>& proposals);
    QVector<Detection> detectPersons(const cv::Mat& frame, cv::Mat& canvas, const std::vector<ObjectProposal>& proposals);
    std::vector<ObjectProposal> inferObjects(const cv::Mat& frame);
    bool claimRecognitionSlot();
    bool isRecognitionReady() const;
    bool isRecognitionRateLimited() const { return recognitionIntervalMs > 0; }
    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const;
    static void normalizeEmbedding(std::vector<float>& embedding);
    bool persistKnownEmbeddings(const QString& path) const;
    void setKnownEmbeddings(const QVector<LabeledEmbedding>& labeledEmbeddings);
    QString saveFacePreview(const QString& name, const cv::Mat& faceBgr) const;
    QString resolvePreviewPath(const QString& storedPath) const;
    static QString generateFaceId();
    bool storeEmbeddingEntry(const QString& name, std::vector<float> embedding, const QString& previewPath, const QString& savePath = QString());
    QString makeAutoLabel();
    void invalidateRecognitionCache();
    bool removeFacePreviewFile(const QString& storedPath) const;

    static cv::Scalar toScalar(const QColor& color);
    static QRect toRect(const cv::Rect& rect, const cv::Size& bounds);
    static cv::Mat imageToMat(const QImage& image);
    static QImage matToImage(const cv::Mat& mat);
    QString detectionCacheKey(int cameraId, const QRect& rect, const QSize& bounds) const;
    bool tryGetCachedRecognition(const QString& key, RecognitionCacheEntry& entry) const;
    void scheduleEmbeddingJob(const QString& key, cv::Mat face);
    void completeRecognitionJob(const QString& key, const RecognitionCacheEntry& entry);
    RecognitionCacheEntry runRecognitionTask(const cv::Mat& face) const;
    QVector<Detection> stabilizeFaces(const QVector<Detection>& rawDetections, const QVector<cv::Mat>& faceCrops, int cameraId);
    struct FaceTrack;
    void applyTrackLabel(FaceTrack& track, const QString& newLabel, float similarity, const QString& previewPath);
    QVector<int> runAssignment(const QVector<QVector<float>>& similarityMatrix) const;
    float intersectionOverUnion(const QRect& a, const QRect& b) const;

    cv::dnn::Net faceNet;
    cv::Ptr<cv::FaceDetectorYN> yuNetDetector;
    FaceDetectorMode faceDetectorMode = FaceDetectorMode::None;
    cv::dnn::Net objectNet;
    cv::HOGDescriptor personHog;

    float faceThreshold = 0.65f;
    float objectThreshold = 0.45f;

    QColor faceColor = QColor(79, 70, 229);
    QColor objectColor = QColor(16, 185, 129);
    QColor recognizedFaceColor = QColor(34, 197, 94); // green for matched embedding
    QColor personColor = QColor(59, 130, 246);
    float recognitionThreshold = 0.60f; // cosine similarity threshold for a match
    float recognitionSoftThreshold = 0.45f; // accept as candidate above this score

    // ONNX Runtime embedding model (ArcFace/MobileFaceNet/SFace/FaceNet)
    std::unique_ptr<AIProcessorONNX> embedEngine;
    std::vector<LabeledEmbedding> knownEmbeddings;
    QString embeddingsPath = QStringLiteral("config/embeddings.json");

    // Auto-enroll unknown faces to embeddings
    bool autoEnrollEnabled = true;
    int autoEnrollCooldownMs = 2000;
    QElapsedTimer autoEnrollTimer;
    int autoEnrollCounter = 1;
    int recognitionIntervalMs = 500;
    mutable QElapsedTimer recognitionTimer;
    int recognitionCacheTtlMs = 500;
    mutable QMutex embeddingMutex;
    mutable QMutex knownEmbeddingsMutex;
    mutable QMutex recognitionCacheMutex;
    QHash<QString, RecognitionCacheEntry> recognitionCache;
    struct FaceTrack {
        quint64 id = 0;
        QRect rect;
        QString stableLabel;
        QString candidateLabel;
        int candidateCount = 0;
        int missCount = 0;
        int framesSinceConfirm = 0;
        bool matchedThisFrame = false;
        bool needsConfirmation = true;
        QString previewPath;
        float lastSimilarity = -1.0f;
    };
    QHash<int, QHash<quint64, FaceTrack>> cameraTracks;
    quint64 nextTrackId = 1;
    int trackMissThreshold = 10;
    int trackConfirmationInterval = 30;
    int hysteresisWindow = 5;
    float trackIouThreshold = 0.4f;
};

Q_DECLARE_METATYPE(AIProcessor::FaceProfile)
Q_DECLARE_METATYPE(QVector<AIProcessor::FaceProfile>)

#endif // AIPROCESSOR_H
