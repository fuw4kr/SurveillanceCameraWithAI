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
    serverSync->sendUnknownAlert(cameraLabel, message);

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
