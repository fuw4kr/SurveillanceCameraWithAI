/**
 * @file AIProcessorONNX.cpp
 * @brief Implements ONNX Runtime-based face embedding extraction with OpenVINO support.
 *
 * Manages session creation, preprocessing, provider selection, and embedding
 * inference for face crops supplied as cv::Mat.
 */
#include "AIProcessorONNX.h"

#include <QDebug>
#include <QProcessEnvironment>

#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>

#include <opencv2/imgproc.hpp>

#include <array>
#include <cmath>
#include <unordered_map>
#include <QMutexLocker>

namespace {
const char* kOrtLoggerName = "ai_processor";

Ort::Env& ortEnv()
{
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, kOrtLoggerName);
    return env;
}

std::string requestedOpenVinoDevice()
{
    const QString envValue = qEnvironmentVariable("AIP_OPENVINO_DEVICE");
    if (!envValue.isEmpty())
        return envValue.toStdString();
#ifdef _WIN32
    return "GPU_FP16";
#else
    return "CPU_FP32";
#endif
}

QString requestedOpenVinoPrecision()
{
    const QString envValue = qEnvironmentVariable("AIP_OPENVINO_PRECISION");
    return envValue.trimmed().toUpper();
}

struct OpenVinoSelection {
    std::string deviceType;
    std::string precision;
    bool usedLegacySuffix = false;
};

OpenVinoSelection normalizeOpenVinoSelection(const std::string& device, const QString& forcedPrecision)
{
    OpenVinoSelection selection;
    QString requested = QString::fromStdString(device).trimmed();
    if (requested.isEmpty())
        requested = QStringLiteral("CPU");

    QString normalized = requested.toUpper();
    const int suffixPos = normalized.indexOf(QStringLiteral("_FP"));
    if (suffixPos >= 0) {
        selection.deviceType = normalized.left(suffixPos).toStdString();
        selection.precision = normalized.mid(suffixPos + 1).toStdString();
        selection.usedLegacySuffix = true;
    } else {
        selection.deviceType = normalized.toStdString();
    }

    if (!forcedPrecision.isEmpty())
        selection.precision = forcedPrecision.toStdString();

    if (selection.precision.empty()) {
        if (normalized.startsWith(QStringLiteral("GPU")) || normalized.startsWith(QStringLiteral("NPU")))
            selection.precision = "FP16";
        else
            selection.precision = "FP32";
    }

    return selection;
}
}

AIProcessorONNX::AIProcessorONNX()
{
    openVinoDeviceType = requestedOpenVinoDevice();
}

bool AIProcessorONNX::shouldUseOpenVino() const
{
    if (!preferOpenVino)
        return false;
    if (qEnvironmentVariableIsSet("AIP_DISABLE_OPENVINO")) {
        qInfo() << "OpenVINO EP disabled via AIP_DISABLE_OPENVINO";
        return false;
    }
#ifdef _WIN32
    return true;
#else
    return qEnvironmentVariableIsSet("AIP_ENABLE_OPENVINO");
#endif
}

void AIProcessorONNX::configureSessionOptions(bool enableOpenVino)
{
    (void)ortEnv();

    embedOptions.reset(new Ort::SessionOptions());
    embedOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    embedOptions->SetIntraOpNumThreads(2);
    embedOptions->SetInterOpNumThreads(1);

    embedUsesOpenVino = enableOpenVino && appendOpenVinoProvider(*embedOptions, openVinoDeviceType);
}

bool AIProcessorONNX::appendOpenVinoProvider(Ort::SessionOptions& options, const std::string& deviceType)
{
    const OpenVinoSelection selection = normalizeOpenVinoSelection(deviceType, requestedOpenVinoPrecision());

    if (selection.usedLegacySuffix) {
        qInfo() << "OpenVINO device request" << QString::fromStdString(deviceType)
                << "remapped to" << QString::fromStdString(selection.deviceType)
                << "with precision" << QString::fromStdString(selection.precision);
    }

#if ORT_API_VERSION >= 17
    try {
        std::unordered_map<std::string, std::string> providerOptions;
        providerOptions["device_type"] = selection.deviceType;
        if (!selection.precision.empty())
            providerOptions["precision"] = selection.precision;
        options.AppendExecutionProvider_OpenVINO_V2(providerOptions);
        qInfo() << "ONNX Runtime: OpenVINO execution provider enabled for device"
                << QString::fromStdString(selection.deviceType)
                << "precision" << QString::fromStdString(selection.precision);
        return true;
    } catch (const Ort::Exception& ex) {
        qWarning() << "ONNX Runtime: OpenVINO V2 provider append failed, retrying legacy API:" << ex.what();
    }
#endif

    try {
        std::string legacyDevice = selection.deviceType;
        if (!selection.precision.empty()) {
            legacyDevice.append("_").append(selection.precision);
        }
        OrtOpenVINOProviderOptions providerOptions{};
        providerOptions.device_type = legacyDevice.c_str();
        providerOptions.enable_dynamic_shapes = 1;
        providerOptions.enable_opencl_throttling = 0;
        providerOptions.enable_npu_fast_compile = 0;
        providerOptions.num_of_threads = 0;
        providerOptions.device_id = nullptr;
        providerOptions.cache_dir = nullptr;
        providerOptions.context = nullptr;
        options.AppendExecutionProvider_OpenVINO(providerOptions);
        qInfo() << "ONNX Runtime: OpenVINO execution provider enabled for device"
                << QString::fromStdString(selection.deviceType)
                << "precision" << QString::fromStdString(selection.precision);
        return true;
    } catch (const Ort::Exception& ex) {
        qWarning() << "ONNX Runtime: OpenVINO provider append failed; CPU will be used:" << ex.what();
        return false;
    }
}

bool AIProcessorONNX::loadModel(const QString& modelPath)
{
    auto loadWithOptions = [&](const Ort::SessionOptions& opts) -> bool {
        const std::wstring wModelPath = modelPath.toStdWString();
        embedSession.reset(new Ort::Session(ortEnv(), wModelPath.c_str(), opts));
        return validateModel(*embedSession);
    };

    embedModelLoaded = false;
    const bool preferOpenVino = shouldUseOpenVino();
    configureSessionOptions(preferOpenVino);

    try {
        if (embedOptions)
            embedModelLoaded = loadWithOptions(*embedOptions);
        if (embedModelLoaded)
            return true;
        if (embedUsesOpenVino)
            qWarning() << "Embedding model validation failed on OpenVINO; trying CPU fallback";
    } catch (const Ort::Exception& ex) {
        qWarning() << "Failed to load embedding model:" << ex.what();
        embedModelLoaded = false;
        embedSession.reset();
    }

    if (embedUsesOpenVino) {
        configureSessionOptions(false);
        try {
            if (embedOptions)
                embedModelLoaded = loadWithOptions(*embedOptions);
            return embedModelLoaded;
        } catch (const Ort::Exception& ex) {
            qWarning() << "CPU fallback for embedding model failed:" << ex.what();
            embedModelLoaded = false;
            embedSession.reset();
        }
    }

    return embedModelLoaded;
}

bool AIProcessorONNX::validateModel(const Ort::Session& session)
{
    Ort::AllocatorWithDefaultOptions alloc;
    Ort::TypeInfo inputInfo = session.GetInputTypeInfo(0);
    auto tensorInfo = inputInfo.GetTensorTypeAndShapeInfo();
    embedInputShape = tensorInfo.GetShape();
    embedRunShape = embedInputShape;

    for (auto& dim : embedRunShape) {
        if (dim <= 0)
            dim = 1;
    }

    if (embedInputShape.size() == 4) {
        embedChannels = static_cast<int>(embedInputShape[1]);
        embedHeight = static_cast<int>(embedInputShape[2]);
        embedWidth = static_cast<int>(embedInputShape[3]);
    } else if (embedInputShape.size() == 3) {
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

bool AIProcessorONNX::preprocessFace(const cv::Mat& face, std::vector<float>& tensor) const
{
    if (face.empty() || embedTensorSize == 0)
        return false;

    cv::cvtColor(face, rgbBuffer, cv::COLOR_BGR2RGB);
    cv::resize(rgbBuffer, resizedBuffer, cv::Size(embedWidth, embedHeight), 0, 0, cv::INTER_AREA);

    tensor.resize(embedTensorSize);
    const size_t channelStride = static_cast<size_t>(embedHeight) * embedWidth;
    for (int y = 0; y < embedHeight; ++y) {
        const uchar* row = resizedBuffer.ptr<uchar>(y);
        for (int x = 0; x < embedWidth; ++x) {
            for (int c = 0; c < embedChannels; ++c) {
                const float v = (static_cast<float>(row[x * 3 + c]) - 127.5f) / 128.0f;
                tensor[c * channelStride + static_cast<size_t>(y) * embedWidth + x] = v;
            }
        }
    }
    return true;
}

std::vector<float> AIProcessorONNX::computeEmbedding(const cv::Mat& faceBgr) const
{
    if (!embedModelLoaded || !embedSession)
        return {};
    if (!preprocessFace(faceBgr, embedInputBuffer))
        return {};

    QMutexLocker locker(&sessionMutex);
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
