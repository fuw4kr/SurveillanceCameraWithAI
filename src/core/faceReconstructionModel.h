#ifndef FACERECONSTRUCTIONMODEL_H
#define FACERECONSTRUCTIONMODEL_H

#include <QMutex>
#include <QSize>
#include <QString>
#include <QVector>
#include <QVector3D>

#include <memory>
#include <string>
#include <vector>

#include <opencv2/core.hpp>

#include <onnxruntime_cxx_api.h>

/**
 * @brief Lightweight wrapper around the 1k3d68.onnx model used for 3D face reconstruction.
 */
class FaceReconstructionModel
{
public:
    FaceReconstructionModel();

    bool loadModel(const QString& modelPath);
    bool isLoaded() const { return modelLoaded; }
    QString lastError() const { return lastErrorMessage; }

    QVector<QVector3D> reconstruct(const cv::Mat& faceBgr, QString* error = nullptr) const;
    QSize inputResolution() const { return QSize(inputWidth, inputHeight); }

private:
    bool preprocess(const cv::Mat& face, std::vector<float>& tensor) const;
    QVector<QVector3D> parseOutput(const std::vector<float>& output) const;

    mutable QMutex sessionMutex;
    bool modelLoaded = false;
    QString lastErrorMessage;

    std::unique_ptr<Ort::Session> session;
    std::unique_ptr<Ort::SessionOptions> sessionOptions;
    std::string inputName;
    std::string outputName;
    std::vector<int64_t> inputShape;
    std::vector<int64_t> outputShape;
    size_t inputTensorSize = 0;
    size_t outputTensorSize = 0;
    int inputChannels = 3;
    int inputWidth = 224;
    int inputHeight = 224;
    mutable std::vector<float> inputBuffer;
};

#endif // FACERECONSTRUCTIONMODEL_H
