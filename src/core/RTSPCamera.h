#ifndef RTSPCAMERA_H
#define RTSPCAMERA_H

/**
 * @file RTSPCamera.h
 * @brief High-level wrapper managing an RTSP camera's video and audio streams.
 *
 * Internally owns FFVideoDecoder and AudioStreamPlayer instances, tracks FPS/online
 * status, and emits frames/status updates to consumers.
 *
 * @example
 * RTSPCamera cam(1, "Entrance", "rtsp://cam/stream");
 * connect(&cam, &RTSPCamera::frameReady, this, &Controller::onFrame);
 * cam.start();
 */

#include <QObject>
#include <QImage>
#include <QElapsedTimer>
#include <QMutex>

#include <memory>
#include <atomic>

#include "FFVideoDecoder.h"
#include "audioStreamPlayer.h"

/**
 * @brief Manages RTSP decoding, audio playback, and basic health reporting.
 */
class RTSPCamera : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs an RTSP camera wrapper with identifiers and URL.
     * @param id Logical camera id.
     * @param name Human-readable camera name.
     * @param rtspUrl Stream URL.
     * @param parent Optional QObject parent.
     * @throws std::bad_alloc If decoder/audio allocation fails.
     * @example RTSPCamera cam(0, "Lobby", url);
     */
    RTSPCamera(int id, QString name, QString rtspUrl, QObject* parent = nullptr);
    /**
     * @brief Stops streaming and cleans up decoders on destruction.
     * @return void
     * @throws None
     * @example delete camera;
     */
    ~RTSPCamera() override;

    /**
     * @brief Begins video (and optional audio) streaming.
     * @return void
     * @throws None
     * @example start();
     */
    void start();
    /**
     * @brief Stops all streaming activities.
     * @return void
     * @throws None
     * @example stop();
     */
    void stop();

    /**
     * @brief Enables or disables audio playback for this camera.
     * @param enable True to start audio, false to stop.
     * @return void
     * @throws None
     * @example enableAudio(true);
     */
    void enableAudio(bool enable);

    /**
     * @brief Indicates whether the stream is currently online.
     * @return bool Online flag.
     * @throws None
     */
    bool isOnline() const { return online; }
    /**
     * @brief Current frames-per-second estimate.
     * @return double FPS value.
     * @throws None
     */
    double fps() const { return currentFps; }
    /**
     * @brief Returns the logical camera id.
     * @return int Camera id.
     * @throws None
     */
    int id() const { return cameraId; }
    /**
     * @brief Human-readable camera name.
     * @return QString Name.
     * @throws None
     */
    QString name() const { return cameraName; }
    /**
     * @brief RTSP URL associated with this camera.
     * @return QString URL.
     * @throws None
     */
    QString url() const { return rtspUrl; }

    /**
     * @brief Latest frame in cv::Mat format for AI processing.
     * @return cv::Mat Copy of last frame.
     * @throws None
     */
    cv::Mat latestMat() const;
    /**
     * @brief Whether the source is local (file/device) vs network RTSP.
     * @return bool True if local.
     * @throws None
     */
    bool isLocalSource() const;

signals:
    /**
     * @brief Emitted when a new frame is available.
     * @param id Camera identifier.
     * @param image Frame image.
     */
    void frameReady(int id, const QImage& image);
    /**
     * @brief Emitted on stream errors.
     * @param id Camera identifier.
     * @param message Error details.
     */
    void cameraError(int id, const QString& message);
    /**
     * @brief Emitted when online status or FPS changes.
     * @param id Camera identifier.
     * @param online Online flag.
     * @param fps Frames per second.
     */
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
