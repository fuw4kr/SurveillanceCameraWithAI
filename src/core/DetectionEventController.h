#ifndef DETECTIONEVENTCONTROLLER_H
#define DETECTIONEVENTCONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QVector>
#include <QSize>

class AIProcessor;
class ServerSyncManager;
struct Detection;

class DetectionEventController : public QObject
{
    Q_OBJECT

public:
    DetectionEventController(AIProcessor* processor, ServerSyncManager* sync, QObject* parent = nullptr);

private slots:
    void handleFrame(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);

private:
    struct ActiveEvent {
        QString personId;
        int cameraId = -1;
        QDateTime startTime;
        QDateTime lastSeen;
    };

    QString keyFor(const QString& personId, int cameraId) const;
    void flushExpired(const QDateTime& nowUtc);

    AIProcessor* aiProcessor = nullptr;
    ServerSyncManager* serverSync = nullptr;
    QHash<QString, ActiveEvent> activeEvents;
    const int timeoutMs = 2000;
};

#endif // DETECTIONEVENTCONTROLLER_H
