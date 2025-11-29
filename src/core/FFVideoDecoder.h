#ifndef FFVIDEODECODER_H
#define FFVIDEODECODER_H

/**
 * @file FFVideoDecoder.h
 * @brief FFmpeg-based worker that pulls RTSP/video streams and emits QImage frames.
 *
 * Converts frames for UI consumption and keeps a cv::Mat copy for AI pipelines.
 * Designed to run in its own thread managed externally by CameraManager/RTSPCamera.
 *
 * @example
 * auto* decoder = new FFVideoDecoder(1);
 * connect(decoder, &FFVideoDecoder::frameReady, this, &Controller::onFrame);
 * decoder->start("rtsp://cam/stream");
 */

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QSize>

#include <atomic>
#include <thread>

#include <opencv2/core.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

/**
 * @brief Decodes a video stream and publishes frames to Qt/AI consumers.
 *
 * Handles FFmpeg setup, conversion to QImage, and exposes thread-safe access to the
 * latest cv::Mat frame.
 */
class FFVideoDecoder : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a decoder for the given camera id.
     * @param cameraId Logical identifier for emitted frames.
     * @param parent Optional QObject parent.
     * @throws None
     * @example FFVideoDecoder decoder(0);
     */
    explicit FFVideoDecoder(int cameraId, QObject* parent = nullptr);
    /**
     * @brief Stops decoding and joins worker thread on destruction.
     * @return void
     * @throws None
     * @example delete decoder;
     */
    ~FFVideoDecoder() override;

    /**
     * @brief Starts decoding the provided URL in a background thread.
     * @param url Stream URL (RTSP/file).
     * @return void
     * @throws None
     * @example start("rtsp://example");
     */
    void start(const QString& url);
    /**
     * @brief Requests decoding to stop and waits for thread completion.
     * @return void
     * @throws None
     * @example stop();
     */
    void stop();
    /**
     * @brief Indicates whether the worker thread is active.
     * @return bool True if decoding loop is running.
     * @throws None
     * @example if (isRunning()) { ... }
     */
    bool isRunning() const { return running.load(); }

    /**
     * @brief Returns the latest decoded frame for AI processing.
     * @return cv::Mat Copy of last frame; may be empty.
     * @throws None
     * @example cv::Mat mat = currentFrame();
     */
    cv::Mat currentFrame() const;
    /**
     * @brief Native resolution of the latest decoded frame.
     * @return QSize Width/height of last frame.
     * @throws None
     * @example QSize size = nativeSize();
     */
    QSize nativeSize() const;

signals:
    /**
     * @brief Emitted when a new frame is ready.
     * @param id Camera identifier.
     * @param image Converted QImage.
     */
    void frameReady(int id, const QImage& image);
    /**
     * @brief Emitted when decoding fails or stream errors occur.
     * @param id Camera identifier.
     * @param message Error description.
     */
    void errorOccurred(int id, const QString& message);

private:
    void decodingLoop(QString url);
    void decodingLoopLocal(const QString& url);
    QImage matToImage(const cv::Mat& mat) const;
    void storeFrame(const cv::Mat& mat);
    static void ensureFFmpegInitialized();

    int cameraId = -1;
    std::atomic<bool> running{ false };
    std::thread worker;
    QString currentUrl;

    mutable QMutex frameMutex;
    cv::Mat latestFrame;
    QSize lastSize;
};

#endif // FFVIDEODECODER_H
