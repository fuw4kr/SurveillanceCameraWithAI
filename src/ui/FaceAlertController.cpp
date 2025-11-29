/**
 * @file FaceAlertController.cpp
 * @brief Implements face alert prompting, auto-enroll flows, and server uploads.
 *
 * Listens to AIProcessor signals to enqueue unknown faces, throttles repeat prompts,
 * and coordinates submissions of alerts, person records, embeddings, and avatars via
 * ServerSyncManager while driving the UnknownFaceDialog UI.
 *
 * @example
 * FaceAlertController controller(aiProcessor, sync, window);
 * // Signal connections trigger dialog presentation automatically.
 */
#include "FaceAlertController.h"

#include "../core/AIProcessor.h"
#include "../core/ServerSyncManager.h"
#include "dialogs/UnknownFaceDialog.h"
#include <QMetaObject>
#include <QRegularExpression>
#include <QTimer>
#include <algorithm>
#include <cmath>

FaceAlertController::FaceAlertController(AIProcessor* processor,
    ServerSyncManager* sync,
    QWidget* window,
    QObject* parent)
    : QObject(parent)
    , aiProcessor(processor)
    , serverSync(sync)
    , parentWindow(window)
{
    if (aiProcessor) {
        connect(aiProcessor, &AIProcessor::frameProcessed,
            this, &FaceAlertController::handleFrame, Qt::QueuedConnection);
        connect(aiProcessor, &AIProcessor::faceAutoEnrolled,
            this, &FaceAlertController::handleAutoEnroll, Qt::QueuedConnection);
    }
    if (serverSync) {
        connect(serverSync, &ServerSyncManager::alertSubmitted,
            this, &FaceAlertController::handleAlertSubmitted);
        connect(serverSync, &ServerSyncManager::alertSubmissionFailed,
            this, &FaceAlertController::handleAlertFailed);
        connect(serverSync, &ServerSyncManager::personSubmitted,
            this, &FaceAlertController::handlePersonSubmitted);
        connect(serverSync, &ServerSyncManager::personSubmissionFailed,
            this, &FaceAlertController::handlePersonFailed);
        connect(serverSync, &ServerSyncManager::embeddingUploaded,
            this, &FaceAlertController::handleEmbeddingUploaded);
        connect(serverSync, &ServerSyncManager::embeddingUploadFailed,
            this, &FaceAlertController::handleEmbeddingFailed);
        connect(serverSync, &ServerSyncManager::avatarUploaded,
            this, &FaceAlertController::handleAvatarUploaded);
        connect(serverSync, &ServerSyncManager::avatarUploadFailed,
            this, &FaceAlertController::handleAvatarUploadFailed);
        connect(serverSync, &ServerSyncManager::personsUpdated,
            this, &FaceAlertController::handlePersonsDirectoryUpdated);
    }
}

void FaceAlertController::handleFrame(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize)
{
    Q_UNUSED(cameraId);
    Q_UNUSED(annotated);
    Q_UNUSED(detections);
    Q_UNUSED(sourceSize);
}

/**
 * @brief Queues an auto-enrolled face snapshot for presentation and upload.
 *
 * Creates a PendingFaceAlert with the supplied label, embedding, and preview image,
 * defaulting camera metadata to placeholders because the source is implicit.
 *
 * @param label Label suggested by the auto-enroll process.
 * @param embedding Embedding vector to be attached to the person record.
 * @param preview Snapshot image of the detected face.
 * @return void
 * @throws None
 * @example handleAutoEnroll("AutoEnroll", vector, previewImage);
 */
void FaceAlertController::handleAutoEnroll(const QString& label, const QVector<float>& embedding, const QImage& preview)
{
    if (!serverSync || preview.isNull())
        return;

    PendingFaceAlert alert;
    alert.cameraId = -1;
    alert.rect = QRect();
    alert.snapshot = preview;
    alert.detectedAt = QDateTime::currentDateTime();
    alert.confidence = 1.0f;
    alert.personLabel = label;
    alert.cameraLabel = tr("Auto-enroll");
    alert.embedding = embedding;
    enqueueFace(alert);
}

void FaceAlertController::handleAlertSubmitted()
{
    qInfo() << "[FaceAlert]" << "Server acknowledged unknown-face alert";
}

void FaceAlertController::handleAlertFailed(const QString& error)
{
    qWarning() << "[FaceAlert]" << "Failed to submit alert:" << error;
}

void FaceAlertController::handlePersonSubmitted(const PersonRecord& person)
{
    qInfo() << "[FaceAlert]" << "Server added person:" << person.name;
    if (awaitingPersonCreation) {
        embeddingPending = !pendingKnownAlert.embedding.isEmpty();
        avatarPending = !pendingKnownAlert.snapshot.isNull();
        if (activeDialog)
            activeDialog->setBusyState(tr("Завантажуємо embedding та фото..."));
        if (embeddingPending)
            serverSync->uploadEmbedding(person.id, QStringLiteral("AutoEnroll"), pendingKnownAlert.embedding);
        if (avatarPending)
            serverSync->uploadPersonAvatar(person.id, pendingKnownAlert.snapshot);
        QTimer::singleShot(1000, serverSync, &ServerSyncManager::requestImmediatePersonsRefresh);
        QTimer::singleShot(1000, serverSync, &ServerSyncManager::requestEmbeddingsRefresh);
        if (!embeddingPending && !avatarPending) {
            awaitingPersonCreation = false;
            if (activeDialog) {
                activeDialog->showSuccess(tr("Збережено на сервері."));
                QTimer::singleShot(700, activeDialog, &QDialog::accept);
            }
        }
    } else {
        serverSync->requestImmediatePersonsRefresh();
    }
}

void FaceAlertController::handlePersonFailed(const QString& error)
{
    qWarning() << "[FaceAlert]" << "Failed to submit person:" << error;
    awaitingPersonCreation = false;
    if (activeDialog)
        activeDialog->showError(error);
}

void FaceAlertController::handleEmbeddingUploaded()
{
    qInfo() << "[FaceAlert]" << "Embedding uploaded for new person";
    if (awaitingPersonCreation) {
        embeddingPending = false;
        if (activeDialog)
            activeDialog->setBusyState(avatarPending ? tr("Embedding завантажено, чекаємо на фото...")
                                                     : tr("Embedding завантажено. Оновлюємо каталог..."));
        if (!avatarPending) {
            awaitingPersonCreation = false;
            if (activeDialog) {
                activeDialog->showSuccess(tr("Збережено на сервері."));
                QTimer::singleShot(700, activeDialog, &QDialog::accept);
            }
        }
    }
}

void FaceAlertController::handleEmbeddingFailed(const QString& error)
{
    qWarning() << "[FaceAlert]" << "Embedding upload failed:" << error;
    awaitingPersonCreation = false;
    if (activeDialog)
        activeDialog->showError(error);
}

void FaceAlertController::handleAvatarUploaded()
{
    qInfo() << "[FaceAlert]" << "Avatar uploaded";
    if (awaitingPersonCreation) {
        avatarPending = false;
        if (activeDialog)
            activeDialog->setBusyState(embeddingPending ? tr("Фото завантажено, чекаємо embedding...")
                                                        : tr("Фото завантажено. Оновлюємо каталог..."));
        if (!embeddingPending) {
            awaitingPersonCreation = false;
            if (activeDialog) {
                activeDialog->showSuccess(tr("Збережено на сервері."));
                QTimer::singleShot(700, activeDialog, &QDialog::accept);
            }
        }
    }
}

void FaceAlertController::handleAvatarUploadFailed(const QString& error)
{
    qWarning() << "[FaceAlert]" << "Avatar upload failed:" << error;
    awaitingPersonCreation = false;
    if (activeDialog)
        activeDialog->showError(error);
}

/**
 * @brief Enqueues a pending alert and triggers presentation.
 * @param alert Pending face alert to display and process.
 * @return void
 * @throws None
 * @example enqueueFace(alert);
 */
void FaceAlertController::handlePersonsDirectoryUpdated(const QList<PersonRecord>& persons)
{
    QRegularExpression rx(QStringLiteral("^UNKNOWN_(\\d+)$"), QRegularExpression::CaseInsensitiveOption);
    for (const auto& person : persons) {
        const auto match = rx.match(person.name);
        if (!match.hasMatch())
            continue;
        bool ok = false;
        const int value = match.captured(1).toInt(&ok);
        if (ok)
            unknownCounter = std::max(unknownCounter, value + 1);
    }
}

QString FaceAlertController::nextUnknownLabel()
{
    const int maxAttempts = 10000;
    QString candidate;
    int attempts = 0;
    do {
        candidate = QStringLiteral("UNKNOWN_%1").arg(unknownCounter++);
        ++attempts;
    } while (serverSync && !serverSync->personIdForName(candidate).isEmpty() && attempts < maxAttempts);
    return candidate;
}

void FaceAlertController::enqueueFace(const PendingFaceAlert& alert)
{
    pendingQueue.enqueue(alert);
    presentNext();
}

/**
 * @brief Checks if a similar face was recently prompted to avoid duplicate dialogs.
 *
 * Compares the center of the detection rectangle against recent prompts within a
 * temporal and spatial threshold.
 *
 * @param cameraId Camera identifier emitting the detection.
 * @param rect Detection bounding box in source coordinates.
 * @param nowMs Current timestamp in milliseconds.
 * @return bool True if a prompt was recently shown for a similar region.
 * @throws None
 * @example bool skip = isRecentlyPrompted(1, faceRect, now);
 */
bool FaceAlertController::isRecentlyPrompted(int cameraId, const QRect& rect, qint64 nowMs) const
{
    const QPoint center = rect.center();
    for (const RecentFace& recent : recentFaces) {
        if (recent.cameraId != cameraId)
            continue;
        if (nowMs - recent.timestampMs > 4000)
            continue;
        if (std::abs(recent.center.x() - center.x()) < 60
            && std::abs(recent.center.y() - center.y()) < 60)
            return true;
    }
    return false;
}

/**
 * @brief Removes stale prompt entries outside the configured time window.
 * @param nowMs Current timestamp in milliseconds.
 * @return void
 * @throws None
 * @example pruneRecent(QDateTime::currentMSecsSinceEpoch());
 */
void FaceAlertController::pruneRecent(qint64 nowMs)
{
    const qint64 threshold = nowMs - 6000;
    auto it = recentFaces.begin();
    while (it != recentFaces.end()) {
        if (it->timestampMs < threshold)
            it = recentFaces.erase(it);
        else
            ++it;
    }
}

/**
 * @brief Presents the next pending face alert in a modal dialog.
 *
 * Skips presentation when another dialog is active, binds dialog signals for
 * unknown/known actions, and schedules processing of the next queued alert once
 * the current dialog is closed.
 *
 * @return void
 * @throws None
 * @example presentNext();
 */
void FaceAlertController::presentNext()
{
    if (dialogActive || pendingQueue.isEmpty())
        return;

    dialogActive = true;
    const PendingFaceAlert alert = pendingQueue.dequeue();

    const QString cameraLabel = !alert.cameraLabel.isEmpty()
        ? alert.cameraLabel
        : tr("Camera %1").arg(alert.cameraId);
    auto* dialog = new UnknownFaceDialog(alert.snapshot, cameraLabel, alert.confidence, parentWindow);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    activeDialog = dialog;

    connect(dialog, &UnknownFaceDialog::markUnknown, this, [this, alert]() {
        handleUnknownSelection(alert);
    });
    connect(dialog, &UnknownFaceDialog::savePerson, this, [this, alert](const QString& name, const QString& role, bool authorized) {
        handleKnownSelection(alert, name, role, authorized);
    });
    connect(dialog, &UnknownFaceDialog::skipped, this, [this]() {
        qInfo() << "[FaceAlert]" << "Alert dismissed";
    });
    connect(dialog, &QObject::destroyed, this, [this]() {
        dialogActive = false;
        activeDialog = nullptr;
        QTimer::singleShot(0, this, &FaceAlertController::presentNext);
    });

    dialog->open();
}

/**
 * @brief Sends an unknown-face alert to the server without enrollment.
 *
 * Formats a descriptive message including camera label and timestamp before
 * delegating submission to the server sync manager.
 *
 * @param alert Pending face alert metadata.
 * @return void
 * @throws None
 * @example handleUnknownSelection(alert);
 */
void FaceAlertController::handleUnknownSelection(const PendingFaceAlert& alert)
{
    if (!serverSync)
        return;
    const QString cameraLabel = !alert.cameraLabel.isEmpty()
        ? alert.cameraLabel
        : tr("Camera %1").arg(alert.cameraId);
    const QString message = tr("Unknown face detected on %1 at %2")
                                .arg(cameraLabel,
                                    alert.detectedAt.toString(Qt::ISODate));
    serverSync->sendUnknownAlert(cameraLabel, message, alert.snapshot);

    if (alert.embedding.isEmpty()) {
        qWarning() << "[FaceAlert]" << "Cannot auto-register unknown face without embedding";
        return;
    }
    const QString unknownName = nextUnknownLabel();
    if (unknownName.isEmpty()) {
        qWarning() << "[FaceAlert]" << "Failed to allocate UNKNOWN label";
        return;
    }
    qInfo() << "[FaceAlert]" << "Registering unknown person as" << unknownName;
    if (activeDialog)
        activeDialog->setBusyState(tr("Registering %1 as unknown...").arg(unknownName));
    handleKnownSelection(alert, unknownName, QStringLiteral("unknown"), false);
}

/**
 * @brief Submits a new person record and queues embedding/avatar uploads.
 *
 * Copies relevant alert data for use after the person record is created, marks
 * the controller as awaiting person creation, and requests server submission.
 *
 * @param alert Pending face alert data selected as a known person.
 * @param name Person's name provided by the operator.
 * @param role Optional role/department label.
 * @param authorized Whether the person is authorized.
 * @return void
 * @throws None
 * @example handleKnownSelection(alert, "Alice", "Staff", true);
 */
void FaceAlertController::handleKnownSelection(const PendingFaceAlert& alert, const QString& name, const QString& role, bool authorized)
{
    if (!serverSync)
        return;
    if (alert.embedding.isEmpty()) {
        qWarning() << "[FaceAlert]" << "No embedding available for" << alert.personLabel;
        return;
    }
    pendingKnownAlert = alert;
    pendingName = name;
    pendingRole = role;
    pendingAuthorized = authorized;
    awaitingPersonCreation = true;
    qInfo() << "[FaceAlert]" << "Submitting new person record for auto-enroll" << alert.personLabel;
    serverSync->submitPersonRecord(name, role, authorized);
}
