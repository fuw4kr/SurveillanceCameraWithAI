#ifndef RTSPCAMERA_H
#define RTSPCAMERA_H

#include <QObject>
#include <QImage>
#include <QElapsedTimer>
#include <QMutex>

#include <memory>
#include <atomic>

#include "FFVideoDecoder.h"
#include "audioStreamPlayer.h"

class RTSPCamera : public QObject
{
    Q_OBJECT
public:
    RTSPCamera(int id, QString name, QString rtspUrl, QObject* parent = nullptr);
    ~RTSPCamera() override;

    void start();
    void stop();

    void enableAudio(bool enable);

    bool isOnline() const { return online; }
    double fps() const { return currentFps; }
    int id() const { return cameraId; }
    QString name() const { return cameraName; }
    QString url() const { return rtspUrl; }

    cv::Mat latestMat() const;
    bool isLocalSource() const;

signals:
    void frameReady(int id, const QImage& image);
    void cameraError(int id, const QString& message);
    void statusChanged(int id, bool online, double fps);

private:
    void handleFrame(int id, const QImage& image);
    void refreshFps();

    int cameraId;
    QString cameraName;
    QString rtspUrl;

    std::unique_ptr<FFVideoDecoder> videoDecoder;
    std::unique_ptr<AudioStreamPlayer> audioPlayer;

    std::atomic<bool> online{ false };
    bool audioEnabled = false;
    int frameSkipMod = 2;      // process every Nth frame for heavy AI
    int frameCounter = 0;
    std::atomic_flag frameInFlight = ATOMIC_FLAG_INIT; // prevent event queue buildup

    QElapsedTimer fpsTimer;
    int framesInInterval = 0;
    double currentFps = 0.0;
};

#endif // RTSPCAMERA_H
