#ifndef AIPROCESSOR_H
#define AIPROCESSOR_H

#include <QObject>
#include <QVector>
#include <QRect>
#include <QString>
#include <QColor>
#include <QElapsedTimer>

#include <onnxruntime_cxx_api.h>
#include <memory>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>

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

    bool loadFaceModel(const QString& modelPath, const QString& configPath = QString());
    bool loadObjectModel(const QString& modelPath, const QString& configPath = QString());
    void setFaceConfidence(float threshold);
    void setObjectConfidence(float threshold);
    void resetBackground();
    bool loadKnownEmbeddings(const QString& jsonPath); // e.g. config/embeddings.json
    bool addKnownEmbedding(const QString& name, const cv::Mat& faceBgr, const QString& savePath = QString());

    bool loadEmbedModel(const QString& modelPath);
    bool hasEmbedModel() const { return embedModelLoaded; }
    std::vector<float> computeEmbedding(const cv::Mat& faceBgr) const;

    ProcessedFrame processFrame(const cv::Mat& frame);

signals:
    void detectionsReady(const QVector<Detection>& detections);

private:
    struct LabeledEmbedding {
        QString name;
        std::vector<float> embedding;
        QString previewPath;
    };

    QVector<Detection> detectFaces(const cv::Mat& frame, cv::Mat& canvas);
    QVector<Detection> detectObjects(const cv::Mat& frame, cv::Mat& canvas);
    QVector<Detection> detectPersons(const cv::Mat& frame, cv::Mat& canvas);
    bool preprocessFace(const cv::Mat& face, std::vector<float>& tensor) const;
    bool validateEmbedModel(const Ort::Session& session);
    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const;
    static void normalizeEmbedding(std::vector<float>& embedding);
    bool persistKnownEmbeddings(const QString& path) const;
    void setKnownEmbeddings(const QVector<LabeledEmbedding>& labeledEmbeddings);
    QString saveFacePreview(const QString& name, const cv::Mat& faceBgr) const;
    QString resolvePreviewPath(const QString& storedPath) const;

    static cv::Scalar toScalar(const QColor& color);
    static QRect toRect(const cv::Rect& rect, const cv::Size& bounds);

    cv::dnn::Net faceNet;
    cv::dnn::Net objectNet;
    cv::HOGDescriptor personHog;

    float faceThreshold = 0.55f;
    float objectThreshold = 0.45f;

    QColor faceColor = QColor(79, 70, 229);
    QColor objectColor = QColor(16, 185, 129);
    QColor recognizedFaceColor = QColor(34, 197, 94); // green for matched embedding
    QColor personColor = QColor(59, 130, 246);
    float recognitionThreshold = 0.38f; // cosine similarity threshold for a match

    // ONNX Runtime embedding model (ArcFace/MobileFaceNet/SFace/FaceNet)
    bool embedModelLoaded = false;
    bool embedUsesDirectML = false;
    std::unique_ptr<Ort::Session> embedSession;
    Ort::SessionOptions embedOptions;
    std::string embedInputName;
    std::string embedOutputName;
    std::vector<int64_t> embedInputShape;
    std::vector<int64_t> embedRunShape; // input shape with dynamic dims resolved to 1 for feeding tensors
    size_t embedTensorSize = 0;
    int embedChannels = 3;
    int embedHeight = 112;
    int embedWidth = 112;
    mutable std::vector<float> embedInputBuffer;
    std::vector<LabeledEmbedding> knownEmbeddings;
    QString embeddingsPath = QStringLiteral("config/embeddings.json");

    // Auto-enroll unknown faces to embeddings
    bool autoEnrollEnabled = true;
    int autoEnrollCooldownMs = 2000;
    QElapsedTimer autoEnrollTimer;
};

#endif // AIPROCESSOR_H
