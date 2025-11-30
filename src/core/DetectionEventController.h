#ifndef DETECTIONEVENTCONTROLLER_H
#define DETECTIONEVENTCONTROLLER_H

/**
 * @file DetectionEventController.h
 * @brief Aggregates detection events and forwards them to the server with debouncing.
 *
 * Listens to AIProcessor frame signals, groups detections into active sessions per
 * person/camera, and sends status updates via ServerSyncManager while pruning stale
 * events.
 *
 * @example
 * auto* controller = new DetectionEventController(ai, sync, this);
 * connect(ai, &AIProcessor::frameProcessed, controller, &DetectionEventController::handleFrame);
 */

#include <QObject>
#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QVector>
#include <QSize>

#include "AIProcessor.h"

class AIProcessor;
class ServerSyncManager;
struct Detection;

/**
 * @brief Debounces detection events and notifies the server of active/inactive states.
 *
 * Tracks active detections per person and camera, expiring them after a timeout to
 * avoid flooding the backend with duplicate events.
 */
class DetectionEventController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Wires AIProcessor and ServerSyncManager signals for detection tracking.
     * @param processor AI pipeline emitting processed frames.
     * @param sync Server sync manager for posting events.
     * @param parent Optional QObject parent.
     * @throws None
     * @example DetectionEventController ctrl(ai, sync, this);
     */
    DetectionEventController(AIProcessor* processor, ServerSyncManager* sync, QObject* parent = nullptr, int timeoutMs = 2000);

private slots:
    /**
     * @brief Processes each annotated frame to update active detection events.
     *
     * Evaluates detections, starts or refreshes active events, and sends status to
     * the server when events expire.
     *
     * @param cameraId Originating camera identifier.
     * @param annotated Annotated frame image (unused here).
     * @param detections Face/person detections for the frame.
     * @param sourceSize Original frame size.
     * @return void
     * @throws None
     * @example handleFrame(0, annotatedImage, detections, source.size());
     */
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
    int timeoutMs = 2000;
};

#endif // DETECTIONEVENTCONTROLLER_H
