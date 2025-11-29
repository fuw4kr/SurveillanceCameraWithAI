#ifndef FACEALERTCONTROLLER_H
#define FACEALERTCONTROLLER_H

#include <QObject>
#include "../core/AIProcessor.h"
#include "../core/ServerTypes.h"
#include <QQueue>
#include <QImage>
#include <QRect>
#include <QDateTime>
#include <QVector>
#include <QSize>
#include <QPoint>
#include <QString>

class QWidget;
class ServerSyncManager;
class UnknownFaceDialog;
struct PersonRecord;

struct PendingFaceAlert {
    int cameraId = -1;
    QRect rect;
    QImage snapshot;
    QDateTime detectedAt;
    float confidence = 0.0f;
    QString personLabel;
    QString cameraLabel;
    QVector<float> embedding;
};

class FaceAlertController : public QObject
{
    Q_OBJECT
public:
    FaceAlertController(AIProcessor* processor,
        ServerSyncManager* sync,
        QWidget* parentWindow,
        QObject* parent = nullptr);

private slots:
    void handleFrame(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);
    void handleAutoEnroll(const QString& label, const QVector<float>& embedding, const QImage& preview);
    void handleAlertSubmitted();
    void handleAlertFailed(const QString& error);
    void handlePersonSubmitted(const PersonRecord& person);
    void handlePersonFailed(const QString& error);
    void handleEmbeddingUploaded();
    void handleEmbeddingFailed(const QString& error);
    void handleAvatarUploaded();
    void handleAvatarUploadFailed(const QString& error);
    void handlePersonsDirectoryUpdated(const QList<PersonRecord>& persons);

private:
    struct RecentFace {
        int cameraId = -1;
        QPoint center;
        qint64 timestampMs = 0;
    };

    void enqueueFace(const PendingFaceAlert& alert);
    bool isRecentlyPrompted(int cameraId, const QRect& rect, qint64 nowMs) const;
    void pruneRecent(qint64 nowMs);
    void presentNext();
    void handleUnknownSelection(const PendingFaceAlert& alert);
    void handleKnownSelection(const PendingFaceAlert& alert, const QString& name, const QString& role, bool authorized);
    QString nextUnknownLabel();

    AIProcessor* aiProcessor = nullptr;
    ServerSyncManager* serverSync = nullptr;
    QWidget* parentWindow = nullptr;
    QQueue<PendingFaceAlert> pendingQueue;
    QVector<RecentFace> recentFaces;
    bool dialogActive = false;
    UnknownFaceDialog* activeDialog = nullptr;
    PendingFaceAlert pendingKnownAlert;
    QString pendingName;
    QString pendingRole;
    bool pendingAuthorized = false;
    bool awaitingPersonCreation = false;
    bool embeddingPending = false;
    bool avatarPending = false;
    int unknownCounter = 1;
};

#endif // FACEALERTCONTROLLER_H
