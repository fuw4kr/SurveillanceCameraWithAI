/**
 * @file RTSPCamera.cpp
 * @brief Implements RTSP camera wrapper coordinating video/audio and status.
 */
#include "RTSPCamera.h"

#include <QDebug>

RTSPCamera::RTSPCamera(int id, QString name, QString url, QObject* parent)
    : QObject(parent)
    , cameraId(id)
    , cameraName(std::move(name))
    , rtspUrl(std::move(url))
    , videoDecoder(new FFVideoDecoder(id, this))
    , audioPlayer(new AudioStreamPlayer(rtspUrl, this))
{
    fpsTimer.start();

    connect(videoDecoder.get(), &FFVideoDecoder::frameReady,
            this, &RTSPCamera::handleFrame);
    connect(videoDecoder.get(), &FFVideoDecoder::errorOccurred, this, [this](int, const QString& msg) {
        emit cameraError(cameraId, msg);
        online = false;
        emit statusChanged(cameraId, online.load(), currentFps);
    });

    connect(audioPlayer.get(), &AudioStreamPlayer::errorOccurred, this, [this](const QString& err) {
        emit cameraError(cameraId, tr("Audio: %1").arg(err));
    });
}

RTSPCamera::~RTSPCamera()
{
    stop();
}

void RTSPCamera::start()
{
    if (!videoDecoder->isRunning())
        videoDecoder->start(rtspUrl);

    if (!isLocalSource() && audioEnabled && !audioPlayer->isRunning())
        audioPlayer->start(); 
}

void RTSPCamera::stop()
{
    videoDecoder->stop();
    audioPlayer->stop();
    online = false;
    emit statusChanged(cameraId, online.load(), currentFps);
}

void RTSPCamera::enableAudio(bool enable)
{
    audioEnabled = enable && !isLocalSource();
    if (audioEnabled) {
        if (!audioPlayer->isRunning())
            audioPlayer->start(); 
    }
    else {
        audioPlayer->stop();
    }
}

cv::Mat RTSPCamera::latestMat() const
{
    return videoDecoder->currentFrame();
}

bool RTSPCamera::isLocalSource() const
{
    return rtspUrl.startsWith(QStringLiteral("local://"), Qt::CaseInsensitive);
}

void RTSPCamera::handleFrame(int id, const QImage& image)
{
    Q_UNUSED(id);
    frameCounter++;
    const bool skipForAi = (frameCounter % frameSkipMod != 0);
    framesInInterval++;
    refreshFps();
    online = true;
    if (!skipForAi) {
        // Drop frame if previous emit is still queued to avoid unbounded growth.
        if (frameInFlight.test_and_set())
            return;

        QImage copy = image.copy(); // detach for queued delivery
        QMetaObject::invokeMethod(this, [this, copy = std::move(copy)]() mutable {
            emit frameReady(cameraId, copy);
            frameInFlight.clear();
        }, Qt::QueuedConnection);
    }
    emit statusChanged(cameraId, online.load(), currentFps);
}

void RTSPCamera::refreshFps()
{
    if (fpsTimer.elapsed() >= 1000) {
        currentFps = framesInInterval / (fpsTimer.elapsed() / 1000.0);
        framesInInterval = 0;
        fpsTimer.restart();
    }
}
