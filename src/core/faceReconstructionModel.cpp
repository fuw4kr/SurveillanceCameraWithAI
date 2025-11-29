/**
 * @file faceReconstructionModel.cpp
 * @brief Implements 3D face mesh reconstruction using an ONNX model.
 */
#include "faceReconstructionModel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <onnxruntime_c_api.h>

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>

namespace {
Ort::Env& ortEnv()
{
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "face_reconstruction");
    return env;
}

template <typename T>
size_t safeProduct(const std::vector<T>& values)
{
    size_t prod = 1;
    for (T v : values) {
        if (v <= 0)
            continue;
        prod *= static_cast<size_t>(v);
    }
    return prod;
}

std::vector<int64_t> sanitizeShape(const std::vector<int64_t>& shape)
{
    std::vector<int64_t> result = shape;
    for (auto& dim : result) {
        if (dim <= 0)
            dim = 1;
    }
    return result;
}
}

FaceReconstructionModel::FaceReconstructionModel() = default;

bool FaceReconstructionModel::loadModel(const QString& modelPath)
{
    lastErrorMessage.clear();
    modelLoaded = false;

    if (modelPath.isEmpty() || !QFileInfo::exists(modelPath)) {
        lastErrorMessage = QObject::tr("Model file not found: %1").arg(modelPath);
        return false;
    }

    sessionOptions.reset(new Ort::SessionOptions());
    sessionOptions->SetIntraOpNumThreads(2);
    sessionOptions->SetInterOpNumThreads(1);
    sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    try {
        const std::wstring wPath = QDir::toNativeSeparators(modelPath).toStdWString();
        session.reset(new Ort::Session(ortEnv(), wPath.c_str(), *sessionOptions));

        Ort::AllocatorWithDefaultOptions allocator;
        {
            Ort::TypeInfo inputInfo = session->GetInputTypeInfo(0);
            auto tensorInfo = inputInfo.GetTensorTypeAndShapeInfo();
            inputShape = tensorInfo.GetShape();
            inputRunShape = sanitizeShape(inputShape);
            inputTensorSize = safeProduct(inputRunShape);
            if (inputShape.size() >= 4) {
                inputChannels = static_cast<int>(std::max<int64_t>(1, inputShape[inputShape.size() - 3]));
                inputHeight = static_cast<int>(std::max<int64_t>(1, inputShape[inputShape.size() - 2]));
                inputWidth = static_cast<int>(std::max<int64_t>(1, inputShape[inputShape.size() - 1]));
            }
            inputName = session->GetInputNameAllocated(0, allocator).get();
        }
        {
            Ort::TypeInfo outputInfo = session->GetOutputTypeInfo(0);
            auto tensorInfo = outputInfo.GetTensorTypeAndShapeInfo();
            outputShape = tensorInfo.GetShape();
            outputTensorSize = safeProduct(outputShape);
            outputName = session->GetOutputNameAllocated(0, allocator).get();
        }

        modelLoaded = inputTensorSize > 0 && outputTensorSize > 0;
    } catch (const Ort::Exception& ex) {
        lastErrorMessage = QString::fromUtf8(ex.what());
        session.reset();
        return false;
    }

    if (!modelLoaded) {
        lastErrorMessage = QObject::tr("Unexpected tensor dimensions in %1").arg(modelPath);
        session.reset();
    } else {
        qInfo() << "3D face model loaded:" << modelPath
                << "input" << inputWidth << "x" << inputHeight
                << "output elements" << outputTensorSize;
    }
    return modelLoaded;
}

bool FaceReconstructionModel::preprocess(const cv::Mat& face, std::vector<float>& tensor) const
{
    if (face.empty() || inputTensorSize == 0)
        return false;

    cv::Mat rgb;
    cv::cvtColor(face, rgb, cv::COLOR_BGR2RGB);

    cv::Mat resized;
    cv::resize(rgb, resized, cv::Size(inputWidth, inputHeight), 0, 0, cv::INTER_AREA);

    tensor.resize(inputTensorSize);
    const size_t channelStride = static_cast<size_t>(inputWidth) * inputHeight;
    for (int y = 0; y < inputHeight; ++y) {
        const uchar* row = resized.ptr<uchar>(y);
        for (int x = 0; x < inputWidth; ++x) {
            for (int c = 0; c < inputChannels; ++c) {
                const float value = (static_cast<float>(row[x * inputChannels + c]) - 127.5f) / 128.0f;
                tensor[c * channelStride + static_cast<size_t>(y) * inputWidth + x] = value;
            }
        }
    }
    return true;
}

QVector<QVector3D> FaceReconstructionModel::parseOutput(const std::vector<float>& output) const
{
    QVector<QVector3D> points;
    if (output.empty())
        return points;

    const size_t vertexCount = output.size() / 3;
    if (vertexCount == 0)
        return points;

    size_t planarCount = 0;
    if (outputShape.size() >= 3) {
        const int64_t channels = outputShape[outputShape.size() - 2];
        const int64_t count = outputShape.back();
        if (channels == 3 && count > 0)
            planarCount = static_cast<size_t>(count);
    }
    const size_t expectedPoints = planarCount > 0 ? planarCount : static_cast<size_t>(std::min<int64_t>(68, vertexCount));

    const bool interleaved = !outputShape.empty()
        && outputShape.back() == 3;
    const size_t stride = std::min({ expectedPoints, vertexCount, output.size() / 3 });
    points.reserve(static_cast<int>(stride));

    if (interleaved) {
        for (size_t i = 0; i < stride; ++i) {
            const size_t base = i * 3;
            if (base + 2 >= output.size())
                break;
            points.append(QVector3D(output[base], output[base + 1], output[base + 2]));
        }
    } else {
        const size_t xOffset = 0;
        const size_t yOffset = stride;
        const size_t zOffset = stride * 2;
        for (size_t i = 0; i < stride; ++i) {
            if (i + zOffset >= output.size())
                break;
            const float x = output[xOffset + i];
            const float y = output[yOffset + i];
            const float z = output[zOffset + i];
            points.append(QVector3D(x, y, z));
        }
    }

    if (!points.isEmpty()) {
        QVector3D centroid;
        for (const QVector3D& p : points)
            centroid += p;
        centroid /= static_cast<float>(points.size());
        for (QVector3D& p : points)
            p -= centroid;
    }

    return points;
}

QVector<QVector3D> FaceReconstructionModel::reconstruct(const cv::Mat& faceBgr, QString* error) const
{
    QVector<QVector3D> points;
    if (!modelLoaded || !session) {
        if (error)
            *error = QObject::tr("3D model is not loaded");
        return points;
    }
    if (faceBgr.empty()) {
        if (error)
            *error = QObject::tr("Input image is empty");
        return points;
    }

    std::vector<float> tensor;
    if (!preprocess(faceBgr, tensor)) {
        if (error)
            *error = QObject::tr("Failed to preprocess image");
        return points;
    }

    Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtDeviceAllocator, OrtMemTypeDefault);
    std::array<const char*, 1> inputNames { inputName.c_str() };
    std::array<const char*, 1> outputNames { outputName.c_str() };

    QVector<QVector3D> result;
    try {
        QMutexLocker locker(&sessionMutex);
        const std::vector<int64_t>& runShape = inputRunShape.empty() ? inputShape : inputRunShape;
        Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memInfo, tensor.data(),
            tensor.size(), runShape.data(), runShape.size());
        auto output = session->Run(Ort::RunOptions{ nullptr }, inputNames.data(), &inputTensor, 1,
            outputNames.data(), 1);
        if (output.empty() || !output[0].IsTensor()) {
            if (error)
                *error = QObject::tr("Model produced no tensor output");
            return points;
        }
        auto typeInfo = output[0].GetTensorTypeAndShapeInfo();
        const size_t outSize = safeProduct(typeInfo.GetShape());
        std::vector<float> values(outSize);
        float* outData = output[0].GetTensorMutableData<float>();
        std::copy(outData, outData + outSize, values.begin());
        locker.unlock();
        result = parseOutput(values);
    } catch (const Ort::Exception& ex) {
        if (error)
            *error = QString::fromUtf8(ex.what());
        return points;
    }

    if (result.isEmpty() && error)
        *error = QObject::tr("Model returned zero vertices");
    return result;
}
