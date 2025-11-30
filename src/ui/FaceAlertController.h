#ifndef FACEALERTCONTROLLER_H
#define FACEALERTCONTROLLER_H

/**
 * @file FaceAlertController.h
 * @brief Coordinates face alerts, auto-enroll flows, and server submissions.
 *
 * The controller consumes detections from the AI processor, prompts users with
 * an unknown-face dialog, and forwards alerts or new person records to the server
 * synchronization manager. It throttles prompts to avoid overwhelming operators.
 *
 * @example
 * FaceAlertController* alerts = new FaceAlertController(processor, serverSync, mainWindow);
 * // Alerts are shown automatically when AIProcessor emits frameProcessed signals.
 */

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

/**
 * @brief Listens for face detections and manages user-facing alert dialogs.
 *
 * Enqueues detection events, throttles duplicates, presents an interactive dialog
 * for unknown faces, and uploads alerts, person records, embeddings, and avatars
 * through the server sync manager.
 *
 * @example
 * FaceAlertController controller(aiProcessor, syncManager, parentWindow);
 */
class FaceAlertController : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the controller and wires AI/server signals.
     *
     * Connects to AIProcessor events for processed frames and auto-enrollment, and
     * to ServerSyncManager signals for submission results.
     *
     * @param processor AI pipeline emitting face detections and embeddings.
     * @param sync Server synchronization manager used to submit alerts and records.
     * @param parentWindow Parent window used as the dialog owner.
     * @param parent Optional QObject parent for lifetime management.
     * @throws None
     * @example FaceAlertController c(ai, sync, window);
     */
    FaceAlertController(AIProcessor* processor,
        ServerSyncManager* sync,
        QWidget* parentWindow,
        QObject* parent = nullptr);

private slots:
    /**
     * @brief Receives processed frames and decides whether to prompt for unknown faces.
     *
     * Currently a stub; intended to inspect detections, throttle duplicate prompts,
     * and enqueue snapshot alerts.
     *
     * @param cameraId ID of the originating camera.
     * @param annotated Frame annotated with detection overlays.
     * @param detections Detected face metadata for the frame.
     * @param sourceSize Original source frame size for coordinate mapping.
     * @return void
     * @throws None
     * @example handleFrame(0, frame, detections, frame.size());
     */
    void handleFrame(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);
    /**
     * @brief Enqueues an auto-enrolled face for confirmation and server submission.
     *
     * Converts incoming embedding and preview into a PendingFaceAlert and places it
     * into the queue.
     *
     * @param label Suggested label/name for the face.
     * @param embedding Feature vector produced by the model.
     * @param preview Snapshot image associated with the detection.
     * @return void
     * @throws None
     * @example handleAutoEnroll("Visitor", embedding, previewImage);
     */
    void handleAutoEnroll(const QString& label, const QVector<float>& embedding, const QImage& preview);
    /**
     * @brief Reacts to successful unknown-face alert submission.
     * @return void
     * @throws None
     * @example handleAlertSubmitted();
     */
    void handleAlertSubmitted();
    /**
     * @brief Handles failures when submitting unknown-face alerts.
     * @param error Error string from the server.
     * @return void
     * @throws None
     * @example handleAlertFailed("Network error");
     */
    void handleAlertFailed(const QString& error);
    /**
     * @brief Processes successful person record creation responses.
     * @param person Newly created person metadata.
     * @return void
     * @throws None
     * @example handlePersonSubmitted(personRecord);
     */
    void handlePersonSubmitted(const PersonRecord& person);
    /**
     * @brief Handles server errors while creating person records.
     * @param error Description of the failure.
     * @return void
     * @throws None
     * @example handlePersonFailed("Validation error");
     */
    void handlePersonFailed(const QString& error);
    /**
     * @brief Confirms embedding upload success for a newly created person.
     * @return void
     * @throws None
     * @example handleEmbeddingUploaded();
     */
    void handleEmbeddingUploaded();
    /**
     * @brief Handles errors when uploading an embedding.
     * @param error Error message from the upload.
     * @return void
     * @throws None
     * @example handleEmbeddingFailed("Timeout");
     */
    void handleEmbeddingFailed(const QString& error);
    /**
     * @brief Confirms avatar image upload success.
     * @return void
     * @throws None
     * @example handleAvatarUploaded();
     */
    void handleAvatarUploaded();
    /**
     * @brief Handles errors during avatar upload.
     * @param error Error description.
     * @return void
     * @throws None
     * @example handleAvatarUploadFailed("Unsupported format");
     */
    void handleAvatarUploadFailed(const QString& error);
    void handlePersonsDirectoryUpdated(const QList<PersonRecord>& persons);

private:
    friend class FaceAlertControllerTestAccessor;
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
    bool autoApproveEnrollments = false;
};

#endif // FACEALERTCONTROLLER_H
