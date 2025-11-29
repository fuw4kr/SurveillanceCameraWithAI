#include "CameraManager.h"

#include <QCameraDevice>
#include <QMediaDevices>
#include <QMutexLocker>
#include <QDebug>

namespace {
constexpr int kMaxDisplayHeight = 720;
}

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
    const auto devices = QMediaDevices::videoInputs();
    for (int i = 0; i < devices.size(); ++i) {
        QString label = devices.at(i).description();
        if (label.isEmpty())
            label = tr("Camera #%1").arg(i);
        available << label;
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

    auto stream = std::make_shared<CameraStream>();
    stream->source = source.trimmed();
    stream->resolvedSource = normalizeSource(stream->source);
    if (stream->resolvedSource.isEmpty()) {
        qWarning() << "Invalid camera source" << source;
        emit cameraError(id, tr("Invalid camera source: %1").arg(source));
        return false;
    }

    stream->decoder = std::make_unique<FFVideoDecoder>(id);
    connect(stream->decoder.get(), &FFVideoDecoder::frameReady, this,
        [this, id](int, const QImage& frame) {
            emit frameReady(id, prepareFrame(frame));
        },
        Qt::QueuedConnection);
    connect(stream->decoder.get(), &FFVideoDecoder::errorOccurred, this,
        [this, id](int, const QString& message) {
            emit cameraError(id, message);
        },
        Qt::QueuedConnection);

    cameras.insert(id, stream);
    qInfo() << "[CameraManager]" << "Camera" << id << "opened from source" << stream->resolvedSource;
    return true;
}

void CameraManager::startCapture(int id)
{
    std::shared_ptr<CameraStream> stream;
    QString source;
    {
        QMutexLocker locker(&mutex);
        auto it = cameras.find(id);
        if (it == cameras.end())
            return;
        stream = it.value();
        if (!stream || stream->capturing.load())
            return;
        stream->capturing.store(true);
        source = stream->resolvedSource;
    }

    if (stream && stream->decoder)
        stream->decoder->start(source);
    qInfo() << "[CameraManager]" << "Capture started for camera" << id;
}

void CameraManager::stopCapture(int id)
{
    std::shared_ptr<CameraStream> stream;
    {
        QMutexLocker locker(&mutex);
        auto it = cameras.find(id);
        if (it == cameras.end())
            return;
        stream = it.value();
        if (!stream)
            return;
        stream->capturing.store(false);
    }

    if (stream && stream->decoder)
        stream->decoder->stop();
    if (stream && stream->audioPlayer)
        stream->audioPlayer->stop();
    qInfo() << "[CameraManager]" << "Capture stopped for camera" << id;
}

void CameraManager::closeCamera(int id)
{
    std::shared_ptr<CameraStream> stream;
    {
        QMutexLocker locker(&mutex);
        auto it = cameras.find(id);
        if (it == cameras.end())
            return;
        stream = it.value();
        cameras.erase(it);
    }

    if (!stream)
        return;

    stream->capturing.store(false);
    if (stream->decoder)
        stream->decoder->stop();
    if (stream->audioPlayer)
        stream->audioPlayer->stop();
    qInfo() << "[CameraManager]" << "Camera" << id << "closed";
}

void CameraManager::closeAll()
{
    QList<int> ids;
    {
        QMutexLocker locker(&mutex);
        ids = cameras.keys();
    }

    for (int id : ids)
        closeCamera(id);
}

bool CameraManager::isCameraOpen(int id) const
{
    QMutexLocker locker(&mutex);
    return cameras.contains(id);
}

void CameraManager::enableAudio(int id)
{
    AudioStreamPlayer* player = nullptr;
    std::shared_ptr<CameraStream> stream;
    {
        QMutexLocker locker(&mutex);
        auto it = cameras.find(id);
        if (it == cameras.end())
            return;
        stream = it.value();
        if (!stream)
            return;
        if (stream->resolvedSource.startsWith(QStringLiteral("local://"), Qt::CaseInsensitive)) {
            qWarning() << "Audio is not supported for local camera" << id;
            return;
        }
        if (!stream->audioPlayer)
            stream->audioPlayer = std::make_unique<AudioStreamPlayer>(stream->source);
        player = stream->audioPlayer.get();
    }

    if (player && !player->isRunning())
        player->start();
}

void CameraManager::disableAudio(int id)
{
    AudioStreamPlayer* player = nullptr;
    std::shared_ptr<CameraStream> stream;
    {
        QMutexLocker locker(&mutex);
        auto it = cameras.find(id);
        if (it == cameras.end())
            return;
        stream = it.value();
        if (!stream || !stream->audioPlayer)
            return;
        player = stream->audioPlayer.get();
    }

    if (player)
        player->stop();
}

QString CameraManager::normalizeSource(const QString& source) const
{
    const QString trimmed = source.trimmed();
    if (trimmed.isEmpty())
        return {};
    if (trimmed.startsWith(QStringLiteral("local://"), Qt::CaseInsensitive))
        return trimmed;

    bool ok = false;
    const int index = trimmed.toInt(&ok);
    if (ok)
        return QStringLiteral("local://%1").arg(index);
    return trimmed;
}

QImage CameraManager::prepareFrame(const QImage& source) const
{
    if (source.isNull())
        return {};

    if (source.height() <= kMaxDisplayHeight)
        return source;
    return source.scaledToHeight(kMaxDisplayHeight, Qt::SmoothTransformation);
}
