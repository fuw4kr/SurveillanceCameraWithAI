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
#include <QFuture>
#include "AudioStreamPlayer.h"
#include <opencv2/opencv.hpp>

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

signals:
    void frameReady(int id, const QImage& frame);
    void cameraError(int id, const QString& message);

public slots:
    void enableAudio(int id);
    void disableAudio(int id);

private:
    struct CameraStream {
        cv::VideoCapture cap;
        bool active = false;
        QString source;
        AudioStreamPlayer* audioPlayer = nullptr;
    };

    mutable QMutex mutex;
    QMap<int, CameraStream*> cameras;

    void captureLoop(int id);
    static QImage matToQImage(const cv::Mat& mat);
};

#endif // CAMERAMANAGER_H
