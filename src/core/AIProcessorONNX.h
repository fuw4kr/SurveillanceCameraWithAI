#ifndef AIPROCESSOR_ONNX_H
#define AIPROCESSOR_ONNX_H

#include <QString>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include <onnxruntime_cxx_api.h>
#include <QMutex>

class AIProcessorONNX
{
public:
    AIProcessorONNX();

    bool loadModel(const QString& modelPath);
    bool isLoaded() const { return embedModelLoaded; }
    std::vector<float> computeEmbedding(const cv::Mat& faceBgr) const;
    void setPreferOpenVino(bool enable) { preferOpenVino = enable; }
    bool prefersOpenVino() const { return preferOpenVino; }

private:
    bool preprocessFace(const cv::Mat& face, std::vector<float>& tensor) const;
    bool validateModel(const Ort::Session& session);
    void configureSessionOptions(bool enableOpenVino);
    bool appendOpenVinoProvider(Ort::SessionOptions& options, const std::string& deviceType);
    bool shouldUseOpenVino() const;

    bool embedModelLoaded = false;
    bool embedUsesOpenVino = false;
    std::string openVinoDeviceType = "GPU_FP16";
    std::unique_ptr<Ort::Session> embedSession;
    std::unique_ptr<Ort::SessionOptions> embedOptions;
    std::string embedInputName;
    std::string embedOutputName;
    std::vector<int64_t> embedInputShape;
    std::vector<int64_t> embedRunShape;
    size_t embedTensorSize = 0;
    int embedChannels = 3;
    int embedHeight = 112;
    int embedWidth = 112;
    mutable std::vector<float> embedInputBuffer;
    mutable cv::Mat rgbBuffer;
    mutable cv::Mat resizedBuffer;
    mutable QMutex sessionMutex;
    bool preferOpenVino = true;
};

#endif // AIPROCESSOR_ONNX_H
