#ifndef SERVERSYNCMANAGER_H
#define SERVERSYNCMANAGER_H

#include "AIProcessor.h"
#include "ServerTypes.h"
#include "SupabaseClient.h"
#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QHash>
#include <QTimer>
#include <QSize>
#include <QVector>
#include <QUrl>

class ServerSyncManager : public QObject
{
    Q_OBJECT

public:
    explicit ServerSyncManager(QObject* parent = nullptr);

    bool loadConfig(const QString& path = QString());
    void setAiProcessor(AIProcessor* processor);
    void start();
    void requestImmediatePersonsRefresh();
    void setCredentials(const QString& email, const QString& password);
    void applySessionToken(const QString& token, const QDateTime& expiresAt);
    void clearSession();
    void sendUnknownAlert(const QString& cameraLabel, const QString& note);
    void submitPersonRecord(const QString& name, const QString& role, bool authorized);
    QString personIdForName(const QString& name) const;
    void sendDetectionStatus(const QString& personId, int cameraId, bool active, const QDateTime& timestamp);
    void renamePerson(const QString& personId, const QString& newName);
    void updatePersonRole(const QString& personId, const QString& newRole);
    void deletePerson(const QString& personId);
    void requestEmbeddingsRefresh();
    void uploadEmbedding(const QString& personId, const QString& modelName, const QVector<float>& vector);
    void uploadPersonAvatar(const QString& personId, const QImage& image);
    QUrl baseUrl() const { return serverUrl; }

signals:
    void personsUpdated(const QList<PersonRecord>& persons);
    void statusMessage(const QString& status);
    void errorMessage(const QString& error);
    void alertSubmitted();
    void alertSubmissionFailed(const QString& error);
    void personSubmitted(const PersonRecord& person);
    void personSubmissionFailed(const QString& error);
    void embeddingUploaded();
    void embeddingUploadFailed(const QString& error);
    void avatarUploaded();
    void avatarUploadFailed(const QString& error);

private slots:
    void handleLoginResult(const AuthResult& result);
    void handlePersonsFetched(const QList<PersonRecord>& persons);
    void handlePersonsFetchFailed(const QString& error);
    void handleEventPosted(const EventPayload& event);
    void handleEventPostFailed(const EventPayload& event, const QString& error);
    void handleSyncTick();
    void handleFrameProcessed(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);
    void handleAlertPosted();
    void handleAlertFailed(const QString& error);
    void handlePersonCreated(const PersonRecord& person);
    void handlePersonFailed(const QString& error);
    void handlePersonUpdated(const PersonRecord& person);
    void handlePersonUpdateFailed(const QString& error);
    void handlePersonDeleted(const QString& personId);
    void handlePersonDeleteFailed(const QString& error);
    void handleEmbeddingsFetched(const QList<EmbeddingRecord>& embeddings);
    void handleEmbeddingsFetchFailed(const QString& error);
    void handleEmbeddingPosted();
    void handleEmbeddingFailed(const QString& error);
    void handleAvatarUploaded();
    void handleAvatarUploadFailed(const QString& error);

private:
    struct QueuedEvent {
        EventPayload payload;
        int attempts = 0;
    };

    void enqueueDetections(int cameraId, const QVector<Detection>& detections);
    void flushEventQueue();
    void requestPersonsRefresh();
    bool ensureAuthenticated();
    bool writeEmbeddingsFile(const QList<EmbeddingRecord>& embeddings, QString* errorOut = nullptr) const;
    PersonRecord personById(const QString& id) const;

    SupabaseClient* client = nullptr;
    AIProcessor* aiProcessor = nullptr;
    QTimer syncTimer;
    QQueue<QueuedEvent> eventQueue;
    QList<PersonRecord> personsCache;
    QList<EmbeddingRecord> embeddingsCache;
    QHash<QString, PersonRecord> personIndex;
    QHash<QString, PersonRecord> personsById;

    QString email;
    QString password;
    QUrl serverUrl{ QStringLiteral("https://myserver-tc2d.onrender.com") };
    QString configPath;
    QString embeddingsPath;
    int syncIntervalMs = 5000;
    bool configLoaded = false;
    bool loginInProgress = false;
    bool personsRequestActive = false;
    bool eventRequestActive = false;
    bool embeddingsRequestActive = false;
    bool manualCredentialsProvided = false;
    const int maxEventQueueSize = 200;
    const int maxEventAttempts = 3;
};

#endif // SERVERSYNCMANAGER_H
