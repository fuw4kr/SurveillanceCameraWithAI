#ifndef FACERECONSTRUCTIONMODEL_H
#define FACERECONSTRUCTIONMODEL_H

/**
 * @file faceReconstructionModel.h
 * @brief Wrapper around 1k3d68.onnx for 3D face landmark reconstruction.
 *
 * Loads an ONNX model through ONNX Runtime, performs preprocessing, and returns a
 * normalized set of 3D points representing the reconstructed face mesh.
 *
 * @example
 * FaceReconstructionModel model;
 * model.loadModel("assets/models/1k3d68.onnx");
 * QVector<QVector3D> mesh = model.reconstruct(faceMat);
 */

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
 * @brief Provides 3D face reconstruction using an ONNX runtime session.
 *
 * Handles input normalization, inference execution, and parsing output tensor into
 * a Qt-friendly vector of 3D points.
 */
class FaceReconstructionModel
{
public:
    /**
     * @brief Constructs an empty model wrapper; call loadModel before inference.
     * @throws std::bad_alloc If session options allocation fails.
     * @example FaceReconstructionModel model;
     */
    FaceReconstructionModel();

    /**
     * @brief Loads the ONNX model from disk.
     * @param modelPath Path to the ONNX file.
     * @return bool True on successful load.
     * @throws None
     * @example model.loadModel("assets/models/1k3d68.onnx");
     */
    bool loadModel(const QString& modelPath);
    /**
     * @brief Indicates whether the model is successfully loaded.
     * @return bool True if loaded.
     * @throws None
     * @example if (model.isLoaded()) { ... }
     */
    bool isLoaded() const { return modelLoaded; }
    /**
     * @brief Provides the last error message when loading or inference fails.
     * @return QString Human-readable error.
     * @throws None
     * @example qWarning() << model.lastError();
     */
    QString lastError() const { return lastErrorMessage; }

    /**
     * @brief Runs inference to reconstruct 3D points from a face image.
     * @param faceBgr Face crop in BGR cv::Mat format.
     * @param error Optional output for error text.
     * @return QVector<QVector3D> Reconstructed point cloud (empty on failure).
     * @throws None
     * @example auto points = model.reconstruct(faceMat);
     */
    QVector<QVector3D> reconstruct(const cv::Mat& faceBgr, QString* error = nullptr) const;
    /**
     * @brief Returns the expected input resolution for the model.
     * @return QSize Width/height required for preprocessing.
     * @throws None
     * @example QSize res = model.inputResolution();
     */
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
