#include "CameraManager.h"
#include <QtConcurrent/QtConcurrent>
#include <QThread>
#include <QDebug>

CameraManager::CameraManager(QObject* parent)
    : QObject(parent)
{
}

CameraManager::~CameraManager()
{
    closeAll();
}

QStringList CameraManager::listAvailableCameras()
{
    QStringList available;
    for (int i = 0; i < 10; ++i) {
        cv::VideoCapture testCap(i);
        if (testCap.isOpened()) {
            available << QString("Camera #%1").arg(i);
            testCap.release();
        }
    }
    return available;
}

bool CameraManager::openCamera(int id, const QString& source)
{
    QMutexLocker locker(&mutex);

    if (cameras.contains(id)) {
        qWarning() << "Camera already open:" << id;
        return true;
    }

    auto* stream = new CameraStream;
    stream->source = source;

    // === визначаємо тип ===
    bool ok = false;
    int camIndex = source.toInt(&ok);

    if (ok) { // локальна камера
        if (!stream->cap.open(camIndex)) {
            emit cameraError(id, QString("Cannot open local camera #%1").arg(camIndex));
            delete stream;
            return false;
        }
    }
    else { // IP / RTSP камера
        if (!stream->cap.open(source.toStdString())) {
            emit cameraError(id, QString("Cannot open IP camera: %1").arg(source));
            delete stream;
            return false;
        }
    }

    cameras[id] = stream;
    return true;
}

void CameraManager::startCapture(int id)
{
    QMutexLocker locker(&mutex);
    auto* stream = cameras.value(id, nullptr);
    if (!stream || stream->active) return;

    stream->active = true;
    QtConcurrent::run([this, id]() { captureLoop(id); });
}

void CameraManager::stopCapture(int id)
{
    QMutexLocker locker(&mutex);
    auto* stream = cameras.value(id, nullptr);
    if (!stream) return;

    stream->active = false;
}

void CameraManager::closeCamera(int id)
{
    QMutexLocker locker(&mutex);
    auto* stream = cameras.value(id, nullptr);
    if (!stream) return;

    stream->active = false;
    QThread::msleep(50); // невелика пауза для завершення потоку

    if (stream->cap.isOpened())
        stream->cap.release();

    delete stream;
    cameras.remove(id);
}

void CameraManager::closeAll()
{
    QMutexLocker locker(&mutex);
    const auto keys = cameras.keys();
    for (int id : keys)
        closeCamera(id);
    cameras.clear();
}

bool CameraManager::isCameraOpen(int id) const
{
    QMutexLocker locker(&mutex);
    return cameras.contains(id);
}

void CameraManager::captureLoop(int id)
{
    cv::Mat frame;

    while (true) {
        {
            QMutexLocker locker(&mutex);
            if (!cameras.contains(id)) return;
            if (!cameras[id]->active) return;
        }

        auto* stream = cameras.value(id, nullptr);
        if (!stream) break;

        stream->cap >> frame;
        if (frame.empty()) continue;

        QImage img = matToQImage(frame);
        emit frameReady(id, img);

        QThread::msleep(33); // ~30 FPS
    }
}

QImage CameraManager::matToQImage(const cv::Mat& mat)
{
    if (mat.empty()) return {};
    if (mat.type() == CV_8UC3) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_BGR888).copy();
    }
    else if (mat.type() == CV_8UC1) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8).copy();
    }
    else if (mat.type() == CV_8UC4) {
        return QImage(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32).copy();
    }
    return {};
}

void CameraManager::enableAudio(int id)
{
    QMutexLocker locker(&mutex);
    auto* stream = cameras.value(id, nullptr);
    if (!stream) return;

    if (stream->audioPlayer) return;

    stream->audioPlayer = new AudioStreamPlayer(stream->source);
    stream->audioPlayer->start();

    qDebug() << "🎧 Audio enabled for camera" << id;
}

void CameraManager::disableAudio(int id)
{
    QMutexLocker locker(&mutex);
    auto* stream = cameras.value(id, nullptr);
    if (!stream || !stream->audioPlayer) return;

    stream->audioPlayer->stop();
    delete stream->audioPlayer;
    stream->audioPlayer = nullptr;

    qDebug() << "🔇 Audio disabled for camera" << id;
}
