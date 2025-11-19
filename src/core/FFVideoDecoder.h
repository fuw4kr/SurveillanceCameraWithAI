#ifndef FFVIDEODECODER_H
#define FFVIDEODECODER_H

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
 * @brief Worker that pulls RTSP video using FFmpeg and emits QImage frames.
 * Converts frames both for UI (QImage) and AI (cv::Mat clone in original resolution).
 */
class FFVideoDecoder : public QObject
{
    Q_OBJECT
public:
    explicit FFVideoDecoder(int cameraId, QObject* parent = nullptr);
    ~FFVideoDecoder() override;

    void start(const QString& url);
    void stop();
    bool isRunning() const { return running.load(); }

    cv::Mat currentFrame() const;
    QSize nativeSize() const;

signals:
    void frameReady(int id, const QImage& image);
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
