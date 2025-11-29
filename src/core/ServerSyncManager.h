#ifndef SERVERSYNCMANAGER_H
#define SERVERSYNCMANAGER_H

/**
 * @file ServerSyncManager.h
 * @brief Orchestrates synchronization of detections, persons, and embeddings with the backend.
 *
 * Handles authentication, periodic polling, queuing detection events, uploading embeddings
 * and avatars, and broadcasting updates to UI components. Integrates AIProcessor outputs
 * with SupabaseClient REST calls.
 *
 * @example
 * ServerSyncManager sync;
 * sync.setAiProcessor(processor);
 * sync.setCredentials(email, password);
 * sync.start();
 */

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

/**
 * @brief Manages backend communication for detections, alerts, and person records.
 *
 * Queues events to avoid flooding, refreshes person/embedding data, and exposes
 * signals for UI updates.
 */
class ServerSyncManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the sync manager without starting network activity.
     * @param parent Optional QObject parent.
     * @throws std::bad_alloc If timers/queues cannot be allocated.
     * @example ServerSyncManager sync(this);
     */
    explicit ServerSyncManager(QObject* parent = nullptr);

    /**
     * @brief Loads configuration (server URL, sync interval, embeddings path).
     * @param path Optional path to config file; defaults to embedded path.
     * @return bool True if config loaded successfully.
     * @throws None
     * @example sync.loadConfig("config/server.json");
     */
    bool loadConfig(const QString& path = QString());
    /**
     * @brief Connects the AI processor so detections are enqueued for sync.
     * @param processor AIProcessor emitting frameProcessed.
     * @return void
     * @throws None
     * @example sync.setAiProcessor(processor);
     */
    void setAiProcessor(AIProcessor* processor);
    /**
     * @brief Starts periodic synchronization tasks and authentication refresh.
     * @return void
     * @throws None
     * @example sync.start();
     */
    void start();
    /**
     * @brief Immediately requests a refresh of persons data from the server.
     * @return void
     * @throws None
     * @example sync.requestImmediatePersonsRefresh();
     */
    void requestImmediatePersonsRefresh();
    /**
     * @brief Sets manual credentials for server login.
     * @param email User email.
     * @param password User password.
     * @return void
     * @throws None
     * @example sync.setCredentials("user@example.com", "secret");
     */
    void setCredentials(const QString& email, const QString& password);
    /**
     * @brief Applies an existing session token with expiry to bypass login.
     * @param token Bearer token.
     * @param expiresAt Expiration timestamp.
     * @return void
     * @throws None
     * @example sync.applySessionToken(token, expiry);
     */
    void applySessionToken(const QString& token, const QDateTime& expiresAt);
    /**
     * @brief Clears cached session data and auth token.
     * @return void
     * @throws None
     * @example sync.clearSession();
     */
    void clearSession();
    /**
     * @brief Sends an unknown-face alert to the backend.
     * @param cameraLabel Human-readable camera identifier.
     * @param note Message to include.
     * @return void
     * @throws None
     * @example sync.sendUnknownAlert("Entrance", "Unknown face detected");
     */
    void sendUnknownAlert(const QString& cameraLabel, const QString& note);
    /**
     * @brief Submits a new person record for creation.
     * @param name Person name.
     * @param role Optional role/department.
     * @param authorized Whether the person is authorized.
     * @return void
     * @throws None
     * @example sync.submitPersonRecord("Alice", "Staff", true);
     */
    void submitPersonRecord(const QString& name, const QString& role, bool authorized);
    /**
     * @brief Resolves a person id by name if available in the cache.
     * @param name Person name.
     * @return QString Person id or empty string.
     * @throws None
     * @example QString id = sync.personIdForName("Alice");
     */
    QString personIdForName(const QString& name) const;
    /**
     * @brief Sends a detection status event to the server.
     * @param personId Target person id.
     * @param cameraId Camera identifier.
     * @param active True if detection is active, false if ended.
     * @param timestamp Event timestamp.
     * @return void
     * @throws None
     * @example sync.sendDetectionStatus(id, 1, true, QDateTime::currentDateTimeUtc());
     */
    void sendDetectionStatus(const QString& personId, int cameraId, bool active, const QDateTime& timestamp);
    /**
     * @brief Renames a person in the backend.
     * @param personId Person identifier.
     * @param newName Desired name.
     * @return void
     * @throws None
     * @example sync.renamePerson(id, "New Name");
     */
    void renamePerson(const QString& personId, const QString& newName);
    /**
     * @brief Deletes a person record by id.
     * @param personId Person identifier.
     * @return void
     * @throws None
     * @example sync.deletePerson(id);
     */
    void deletePerson(const QString& personId);
    /**
     * @brief Requests latest embeddings from the server.
     * @return void
     * @throws None
     * @example sync.requestEmbeddingsRefresh();
     */
    void requestEmbeddingsRefresh();
    /**
     * @brief Uploads an embedding vector for a person.
     * @param personId Person identifier.
     * @param modelName Embedding model name.
     * @param vector Embedding vector.
     * @return void
     * @throws None
     * @example sync.uploadEmbedding(id, "ArcFace", embedding);
     */
    void uploadEmbedding(const QString& personId, const QString& modelName, const QVector<float>& vector);
    /**
     * @brief Uploads a person avatar image.
     * @param personId Person identifier.
     * @param image Image to upload.
     * @return void
     * @throws None
     * @example sync.uploadPersonAvatar(id, avatarImage);
     */
    void uploadPersonAvatar(const QString& personId, const QImage& image);

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
