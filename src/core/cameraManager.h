/**
 * @file CameraManager.h
 * @brief Manages multiple local and IP cameras using OpenCV + Qt.
 */

#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QMap>
#include <QStringList>
#include <memory>
#include <atomic>

#include "FFVideoDecoder.h"
#include "audioStreamPlayer.h"

class CameraManager : public QObject
{
    Q_OBJECT
public:
    explicit CameraManager(QObject* parent = nullptr);
    ~CameraManager();

    // 🔹 Відкрити камеру (локальну або IP)
    bool openCamera(int id, const QString& source);

    // 🔹 Закрити конкретну або всі
    void closeCamera(int id);
    void closeAll();

    // 🔹 Контроль
    void startCapture(int id);
    void stopCapture(int id);
    bool isCameraOpen(int id) const;

    // 🔹 Для локальних USB камер
    QStringList listAvailableCameras();
    QString cameraSource(int id) const;

signals:
    void frameReady(int id, const QImage& frame);
    void cameraError(int id, const QString& message);

public slots:
    void enableAudio(int id);
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
