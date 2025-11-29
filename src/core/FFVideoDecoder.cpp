/**
 * @file FFVideoDecoder.cpp
 * @brief FFmpeg video decoding worker emitting QImage frames for cameras.
 */
#include "FFVideoDecoder.h"

#include <QThread>
#include <QElapsedTimer>
#include <QDebug>
#include <QByteArray>

#include <array>
#include <vector>
#include <cstring>
#include <mutex>
#include <opencv2/videoio.hpp>

namespace {
std::once_flag g_ffmpegOnce;

inline AVPixelFormat toPixelFormat(AVPixelFormat fmt)
{
    if (fmt == AV_PIX_FMT_NONE)
        return AV_PIX_FMT_YUV420P;
    return fmt;
}

cv::VideoCapture openLocalDevice(int index)
{
    struct BackendAttempt {
        int api;
        const char* description;
    };

    const BackendAttempt attempts[] = {
#ifdef _WIN32
        { cv::CAP_DSHOW, "DirectShow" },
        { cv::CAP_MSMF, "MediaFoundation" },
#endif
        { cv::CAP_ANY, "Auto" },
    };

    for (const auto& attempt : attempts) {
        cv::VideoCapture capture;
        const bool opened = (attempt.api == cv::CAP_ANY)
            ? capture.open(index)
            : capture.open(index, attempt.api);
        if (opened && capture.isOpened())
            return capture;
    }
    return {};
}
} // namespace

FFVideoDecoder::FFVideoDecoder(int cameraId, QObject* parent)
    : QObject(parent)
    , cameraId(cameraId)
{
    // Disable GStreamer for OpenCV capture to avoid missing-pipeline warnings; fall back to MSMF/FFmpeg.
#ifdef _WIN32
    qputenv("OPENCV_VIDEOIO_PRIORITY_GSTREAMER", "0");
    // Intel MSMF+D3D11 path is unstable on some drivers (ControlLib.dll AV); force MSMF to CPU transforms.
    qputenv("OPENCV_VIDEOIO_MSMF_ENABLE_HW_TRANSFORMS", "0");
    qputenv("OPENCV_VIDEOIO_MSMF_D3D11_TEXTURE", "0");
#endif
    ensureFFmpegInitialized();
}

FFVideoDecoder::~FFVideoDecoder()
{
    stop();
}

void FFVideoDecoder::ensureFFmpegInitialized()
{
    std::call_once(g_ffmpegOnce, []() {
        avformat_network_init();
        av_log_set_level(AV_LOG_WARNING);
    });
}

void FFVideoDecoder::start(const QString& url)
{
    stop();
    running = true;
    currentUrl = url;
    worker = std::thread(&FFVideoDecoder::decodingLoop, this, url);
}

void FFVideoDecoder::stop()
{
    running = false;
    if (worker.joinable()) {
        worker.join();
    }
}

cv::Mat FFVideoDecoder::currentFrame() const
{
    QMutexLocker locker(&frameMutex);
    return latestFrame.clone();
}

QSize FFVideoDecoder::nativeSize() const
{
    return lastSize;
}

void FFVideoDecoder::decodingLoop(QString url)
{
    if (url.startsWith(QStringLiteral("local://"), Qt::CaseInsensitive)) {
        decodingLoopLocal(url);
        return;
    }

    constexpr int reconnectDelayMs = 1500;

    while (running) {
        AVFormatContext* fmtCtx = nullptr;
        AVDictionary* inputOptions = nullptr;
        av_dict_set(&inputOptions, "rtsp_transport", "tcp", 0);
        av_dict_set(&inputOptions, "max_delay", "500000", 0); // 0.5 s jitter buffer

        QByteArray urlUtf8 = url.toUtf8();
        const int openResult = avformat_open_input(&fmtCtx, urlUtf8.constData(), nullptr, &inputOptions);
        av_dict_free(&inputOptions);
        if (openResult < 0) {
            emit errorOccurred(cameraId, tr("Unable to open RTSP stream"));
            QThread::msleep(reconnectDelayMs);
            continue;
        }

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            emit errorOccurred(cameraId, tr("Unable to read stream info"));
            avformat_close_input(&fmtCtx);
            QThread::msleep(reconnectDelayMs);
            continue;
        }

        const int videoStreamIndex = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (videoStreamIndex < 0) {
            emit errorOccurred(cameraId, tr("Video stream not found"));
            avformat_close_input(&fmtCtx);
            QThread::msleep(reconnectDelayMs);
            continue;
        }

        AVStream* videoStream = fmtCtx->streams[videoStreamIndex];
        const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
        if (!codec) {
            emit errorOccurred(cameraId, tr("Codec not supported"));
            avformat_close_input(&fmtCtx);
            QThread::msleep(reconnectDelayMs);
            continue;
        }

        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codecCtx, videoStream->codecpar);
        codecCtx->thread_count = 0; // auto

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            emit errorOccurred(cameraId, tr("Failed to open codec"));
            avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
            QThread::msleep(reconnectDelayMs);
            continue;
        }

        AVFrame* frame = av_frame_alloc();
        AVFrame* rgbFrame = av_frame_alloc();
        AVPacket* packet = av_packet_alloc();

        SwsContext* sws = nullptr;
        std::vector<uint8_t> buffer;

        auto cleanup = [&]() {
            if (sws) sws_freeContext(sws);
            av_packet_free(&packet);
            av_frame_free(&rgbFrame);
            av_frame_free(&frame);
            avcodec_free_context(&codecCtx);
            avformat_close_input(&fmtCtx);
        };

        while (running) {
            if (av_read_frame(fmtCtx, packet) < 0) {
                emit errorOccurred(cameraId, tr("Stream read error, reconnecting..."));
                break;
            }

            if (packet->stream_index != videoStreamIndex) {
                av_packet_unref(packet);
                continue;
            }

            int ret = avcodec_send_packet(codecCtx, packet);
            av_packet_unref(packet);
            if (ret < 0) {
                emit errorOccurred(cameraId, tr("Decode error"));
                break;
            }

            while (ret >= 0) {
                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0) {
                    emit errorOccurred(cameraId, tr("Receive frame error"));
                    break;
                }

                const int width = frame->width;
                const int height = frame->height;
                if (width <= 0 || height <= 0)
                    continue;

                if (!sws) {
                    sws = sws_getContext(
                        width, height, toPixelFormat(static_cast<AVPixelFormat>(frame->format)),
                        width, height, AV_PIX_FMT_BGR24,
                        SWS_BICUBIC, nullptr, nullptr, nullptr);
                    if (!sws) {
                        emit errorOccurred(cameraId, tr("Failed to init swscale"));
                        running = false;
                        break;
                    }

                    buffer.resize(static_cast<size_t>(av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1)));
                    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, buffer.data(),
                        AV_PIX_FMT_BGR24, width, height, 1);
                }

                sws_scale(sws, frame->data, frame->linesize, 0, height, rgbFrame->data, rgbFrame->linesize);

                cv::Mat mat(height, width, CV_8UC3, rgbFrame->data[0], rgbFrame->linesize[0]);
                cv::Mat cloned = mat.clone();
                storeFrame(cloned);
                lastSize = QSize(width, height);
                emit frameReady(cameraId, matToImage(cloned));
            }
        }

        cleanup();

        if (running)
            QThread::msleep(reconnectDelayMs);
    }
}

void FFVideoDecoder::decodingLoopLocal(const QString& url)
{
    constexpr int reconnectDelayMs = 1000;
    bool ok = false;
    const int deviceIndex = url.mid(8).toInt(&ok);
    const int index = ok ? deviceIndex : 0;

    while (running) {
        cv::VideoCapture capture = openLocalDevice(index);
        if (!capture.isOpened()) {
            emit errorOccurred(cameraId, tr("Unable to open local camera %1").arg(index));
            QThread::msleep(reconnectDelayMs);
            continue;
        }

        while (running) {
            cv::Mat frame;
            if (!capture.read(frame) || frame.empty()) {
                QThread::msleep(30);
                continue;
            }

            storeFrame(frame);
            lastSize = QSize(frame.cols, frame.rows);
            emit frameReady(cameraId, matToImage(frame));
        }

        capture.release();
        if (running)
            QThread::msleep(reconnectDelayMs);
    }
}

QImage FFVideoDecoder::matToImage(const cv::Mat& mat) const
{
    if (mat.empty())
        return {};

    // OpenCV uses BGR; wrap and copy in one step without per-row loop.
    QImage image(mat.data, mat.cols, mat.rows, static_cast<int>(mat.step), QImage::Format_BGR888);
    return image.copy(); // ensure ownership once; cheaper than row-by-row + rgbSwapped
}

void FFVideoDecoder::storeFrame(const cv::Mat& mat)
{
    QMutexLocker locker(&frameMutex);
    latestFrame = mat.clone();
}
