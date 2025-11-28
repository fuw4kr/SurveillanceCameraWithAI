#ifndef AIPROCESSOR_H
#define AIPROCESSOR_H

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

struct Detection
{
    QRect rect;
    QString label;
    QString category; // e.g. Face/Object/Person for stats
    float confidence = 0.0f;
    QColor color = QColor(255, 255, 255);
    QString previewPath; // optional path to enrolled face crop
};

struct ProcessedFrame
{
    cv::Mat annotated;
    QVector<Detection> detections;
};

Q_DECLARE_METATYPE(Detection)
Q_DECLARE_METATYPE(QVector<Detection>)

/**
 * @brief AIProcessor runs lightweight analytics (face/object/person detection).
 * It is designed as a reusable component that takes cv::Mat frames and annotates them.
 * Detection models (FaceNet / DNN) can be supplied at runtime via loadFaceModel/loadObjectModel.
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

    bool loadFaceModel(const QString& modelPath, const QString& configPath = QString());
    bool loadObjectModel(const QString& modelPath, const QString& configPath = QString());
    int recognitionInterval() const { return recognitionIntervalMs; }
    void resetBackground();
    bool loadKnownEmbeddings(const QString& jsonPath); // e.g. config/embeddings.json
    bool addKnownEmbedding(const QString& name, const cv::Mat& faceBgr, const QString& savePath = QString());

    bool prefersGpuForEmbeddings() const;
    bool loadEmbedModel(const QString& modelPath);
    bool hasEmbedModel() const;
    std::vector<float> computeEmbedding(const cv::Mat& faceBgr) const;

    ProcessedFrame processFrame(const cv::Mat& frame, int cameraId = -1);
    Q_INVOKABLE QVector<FaceProfile> listFaceProfiles() const;
    Q_INVOKABLE bool renameFaceProfile(const QString& id, const QString& newName);
    Q_INVOKABLE bool deleteFaceProfile(const QString& id);
    Q_INVOKABLE bool mergeFaceProfiles(const QString& targetId, const QStringList& duplicateIds);

public slots:
    void setFaceConfidence(float threshold);
    void setObjectConfidence(float threshold);
    void setRecognitionIntervalMs(int intervalMs);
    void setPreferGpuForEmbeddings(bool enable);
    void processFrameAsync(int cameraId, const QImage& frame);
    void loadEmbedModelAsync(const QString& modelPath) { loadEmbedModel(modelPath); }
    void loadFaceModelAsync(const QString& modelPath, const QString& configPath = QString()) { loadFaceModel(modelPath, configPath); }
    void loadObjectModelAsync(const QString& modelPath, const QString& configPath = QString()) { loadObjectModel(modelPath, configPath); }

signals:
    void detectionsReady(const QVector<Detection>& detections);
    void frameProcessed(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);
    void recognitionIntervalChanged(int intervalMs);
    void embeddingBackendChanged(bool preferGpu);
    void faceDatabaseChanged();

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
