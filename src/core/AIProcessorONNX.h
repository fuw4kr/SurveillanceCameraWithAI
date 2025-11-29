#ifndef AIPROCESSOR_ONNX_H
#define AIPROCESSOR_ONNX_H

/**
 * @file AIProcessorONNX.h
 * @brief ONNX Runtime wrapper for face embedding inference.
 *
 * Loads ArcFace/MobileFaceNet-style models, handles preprocessing, and optionally
 * delegates to OpenVINO execution providers. Thread-safe through a session mutex.
 *
 * @example
 * AIProcessorONNX engine;
 * engine.loadModel("assets/models/arcface.onnx");
 * auto embedding = engine.computeEmbedding(faceMat);
 */

#include <QString>
#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include <onnxruntime_cxx_api.h>
#include <QMutex>

/**
 * @brief Performs face embedding extraction using ONNX Runtime with optional OpenVINO.
 */
class AIProcessorONNX
{
public:
    /**
     * @brief Constructs the wrapper with default OpenVINO preference.
     * @throws std::bad_alloc If internal allocations fail.
     */
    AIProcessorONNX();

    /**
     * @brief Loads an ONNX embedding model from disk.
     * @param modelPath Path to the ONNX model.
     * @return bool True on success.
     * @throws None
     */
    bool loadModel(const QString& modelPath);
    /**
     * @brief Indicates whether a model is loaded.
     * @return bool True if the embedding session is ready.
     */
    bool isLoaded() const { return embedModelLoaded; }
    /**
     * @brief Computes an embedding vector for the provided face crop.
     * @param faceBgr Face image in BGR cv::Mat format.
     * @return std::vector<float> Normalized embedding (empty on failure).
     * @throws None
     */
    std::vector<float> computeEmbedding(const cv::Mat& faceBgr) const;
    /**
     * @brief Toggles preference for OpenVINO execution provider.
     * @param enable True to prefer OpenVINO, false to force default CPU.
     * @return void
     * @throws None
     */
    void setPreferOpenVino(bool enable) { preferOpenVino = enable; }
    /**
     * @brief Returns whether OpenVINO is preferred for the next load.
     * @return bool Preference flag.
     */
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
