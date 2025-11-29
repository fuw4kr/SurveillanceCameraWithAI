/**
 * @file CameraManager.h
 * @brief Manages multiple local and IP cameras using OpenCV + Qt.
 */

#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

/**
 * @file CameraManager.h
 * @brief Manages multiple local and IP camera streams using FFmpeg/OpenCV.
 *
 * Provides a registry of camera decoders, emitting frames as QImages for the UI,
 * and controls for starting/stopping capture and toggling audio per camera. Each
 * camera is referenced by an integer id so UI components can address streams
 * independently.
 *
 * @example
 * CameraManager mgr;
 * mgr.openCamera(0, "rtsp://cam/stream");
 * mgr.startCapture(0);
 * connect(&mgr, &CameraManager::frameReady, [](int id, const QImage& frame){ /* ... */ });
 */

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QMap>
#include <QStringList>
#include <memory>
#include <atomic>

#include "FFVideoDecoder.h"
#include "audioStreamPlayer.h"

/**
 * @brief Coordinator for multiple camera streams, video decode, and audio playback.
 *
 * Wraps FFVideoDecoder and AudioStreamPlayer instances, exposing Qt signals for frames
 * and errors while offering convenience methods to open, start, stop, and close streams.
 */
class CameraManager : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the manager without opening any streams.
     * @param parent Optional Qt parent for lifetime management.
     * @throws std::bad_alloc If internal allocations fail.
     * @example CameraManager manager;
     */
    explicit CameraManager(QObject* parent = nullptr);
    /**
     * @brief Closes all open cameras and releases decoders.
     * @return void
     * @throws None
     * @example delete cameraManager;
     */
    ~CameraManager();

    /**
     * @brief Opens a camera source (USB, file, or RTSP) and stores it under the provided id.
     * @param id Logical camera identifier.
     * @param source URL or device path.
     * @return bool True if the source could be opened.
     * @throws None
     * @example openCamera(1, "rtsp://cam/stream");
     */
    bool openCamera(int id, const QString& source);

    /**
     * @brief Stops and removes a camera stream by id.
     * @param id Camera identifier to close.
     * @return void
     * @throws None
     * @example closeCamera(1);
     */
    void closeCamera(int id);
    /**
     * @brief Closes all active cameras and audio players.
     * @return void
     * @throws None
     * @example closeAll();
     */
    void closeAll();

    /**
     * @brief Starts capturing frames for the camera with the given id.
     * @param id Camera identifier.
     * @return void
     * @throws None
     * @example startCapture(0);
     */
    void startCapture(int id);
    /**
     * @brief Stops frame capture for the specified camera.
     * @param id Camera identifier.
     * @return void
     * @throws None
     * @example stopCapture(0);
     */
    void stopCapture(int id);
    /**
     * @brief Checks if a camera is currently open.
     * @param id Camera identifier.
     * @return bool True when the camera is open.
     * @throws None
     * @example bool open = isCameraOpen(2);
     */
    bool isCameraOpen(int id) const;

    /**
     * @brief Lists available local (USB) camera device names.
     * @return QStringList Human-readable camera names.
     * @throws None
     * @example auto cameras = listAvailableCameras();
     */
    QStringList listAvailableCameras();
    QString cameraSource(int id) const;

signals:
    /**
     * @brief Emitted when a new frame is decoded for a camera.
     * @param id Camera identifier.
     * @param frame Frame as QImage.
     */
    void frameReady(int id, const QImage& frame);
    /**
     * @brief Emitted when a camera encounters an error.
     * @param id Camera identifier.
     * @param message Error details.
     */
    void cameraError(int id, const QString& message);

public slots:
    /**
     * @brief Enables audio playback for a camera stream if available.
     * @param id Camera identifier.
     * @return void
     * @throws None
     * @example enableAudio(1);
     */
    void enableAudio(int id);
    /**
     * @brief Disables audio playback for a camera stream.
     * @param id Camera identifier.
     * @return void
     * @throws None
     * @example disableAudio(1);
     */
    void disableAudio(int id);

private:
    struct CameraStream {
        QString source;
        QString resolvedSource;
        std::unique_ptr<FFVideoDecoder> decoder;
        std::unique_ptr<AudioStreamPlayer> audioPlayer;
        std::atomic_bool capturing{ false };
    };

    mutable QMutex mutex;
    QMap<int, std::shared_ptr<CameraStream>> cameras;

    QString normalizeSource(const QString& source) const;
    QImage prepareFrame(const QImage& source) const;
};

#endif // CAMERAMANAGER_H
