/**
 * @file DetectionEventController.cpp
 * @brief Implements debounced detection event tracking and server notifications.
 */
#include "DetectionEventController.h"

#include "AIProcessor.h"
#include "ServerSyncManager.h"
#include <QDebug>

DetectionEventController::DetectionEventController(AIProcessor* processor, ServerSyncManager* sync, QObject* parent, int timeoutMsValue)
    : QObject(parent)
    , aiProcessor(processor)
    , serverSync(sync)
    , timeoutMs(timeoutMsValue)
{
    if (aiProcessor) {
        connect(aiProcessor, &AIProcessor::frameProcessed,
            this, &DetectionEventController::handleFrame,
            Qt::QueuedConnection);
    }
}

QString DetectionEventController::keyFor(const QString& personId, int cameraId) const
{
    return QStringLiteral("%1_%2").arg(personId, QString::number(cameraId));
}

void DetectionEventController::flushExpired(const QDateTime& nowUtc)
{
    auto it = activeEvents.begin();
    while (it != activeEvents.end()) {
        if (it->lastSeen.msecsTo(nowUtc) > timeoutMs) {
            serverSync->sendDetectionStatus(it->personId, it->cameraId, false, it->lastSeen, it->snapshot, it->confidence);
            it = activeEvents.erase(it);
        } else {
            ++it;
        }
    }
}

void DetectionEventController::handleFrame(int cameraId, const QImage&, const QVector<Detection>& detections, const QSize&)
{
    if (!serverSync)
        return;

    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

    for (const Detection& detection : detections) {
        if (detection.category.compare(QStringLiteral("Face"), Qt::CaseInsensitive) != 0)
            continue;
        if (detection.label.isEmpty() || detection.label == QStringLiteral("Face"))
            continue;

        const QString personId = serverSync->personIdForName(detection.label);
        if (personId.isEmpty())
            continue;

        const QString key = keyFor(personId, cameraId);
        auto it = activeEvents.find(key);
        if (it == activeEvents.end()) {
            ActiveEvent evt;
            evt.personId = personId;
            evt.cameraId = cameraId;
            evt.startTime = nowUtc;
            evt.lastSeen = nowUtc;
            QImage snapshot;
            if (!detection.previewPath.isEmpty())
                snapshot = QImage(detection.previewPath);
            if (snapshot.isNull()) {
                qWarning() << "[DetectionEventController]" << "Skipping event start without snapshot for" << detection.label;
                continue;
            }
            evt.snapshot = snapshot;
            evt.confidence = detection.confidence;
            activeEvents.insert(key, evt);
            serverSync->sendDetectionStatus(personId, cameraId, true, nowUtc, snapshot, detection.confidence);
        } else {
            it->lastSeen = nowUtc;
            if (!detection.previewPath.isEmpty()) {
                QImage snapshot(detection.previewPath);
                if (!snapshot.isNull())
                    it->snapshot = snapshot;
            }
            it->confidence = detection.confidence;
        }
    }

    flushExpired(nowUtc);
}
