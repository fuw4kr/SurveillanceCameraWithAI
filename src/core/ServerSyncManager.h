#ifndef SERVERSYNCMANAGER_H
#define SERVERSYNCMANAGER_H

/**
 * @file ServerSyncManager.h
 * @brief Orchestrates synchronization of detections, persons, and embeddings with the backend.
 *
 * Handles authentication, periodic polling, uploading embeddings and avatars, and
 * broadcasting updates to UI components. Integrates AIProcessor outputs with
 * SupabaseClient REST calls and streams debounced detection events to the server.
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
#include <QHash>
#include <QTimer>
#include <QSize>
#include <QVector>
#include <QUrl>

class CameraManager;

/**
 * @brief Manages backend communication for detections, alerts, and person records.
 *
 * Posts detection lifecycle events, refreshes person/embedding data, and exposes
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
    ~ServerSyncManager() override;

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
    void setCameraManager(CameraManager* manager);
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
    void sendUnknownAlert(const QString& cameraLabel, const QString& note, const QImage& snapshot = QImage());
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
    void sendDetectionStatus(const QString& personId,
        int cameraId,
        bool active,
        const QDateTime& timestamp,
        const QImage& snapshot = QImage(),
        float confidence = -1.0f);
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
    void updatePersonRole(const QString& personId, const QString& newRole);
    void deletePerson(const QString& personId);
    /**
     * @brief Requests latest embeddings from the server.
     * @return void
     * @throws None
     * @example sync.requestEmbeddingsRefresh();
     */
    void requestEmbeddingsRefresh();
    void requestImmediateEmbeddingsRefresh();
    /**
     * @brief Uploads an embedding vector for a person.
     * @param personId Person identifier.
     * @param modelName Embedding model name.
     * @param vector Embedding vector.
     * @return void
     * @throws None
     * @example sync.uploadEmbedding(id, "ArcFace", embedding);
     */
    void requestCamerasRefresh();
    void requestImmediateCamerasRefresh();
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
    void submitCameraRecord(const QString& name, const QString& streamUrl, const QString& ipAddress = QString(), const QString& location = QString());
    void deleteCameraRecord(const QString& cameraId);
    void updateCameraStatus(const QString& cameraId, const QString& status);
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
    void camerasUpdated(const QList<CameraRecord>& cameras);
    void cameraSubmitted(const CameraRecord& camera);
    void cameraSubmissionFailed(const QString& error);
    void cameraDeleted(const QString& cameraId);
    void cameraDeleteFailed(const QString& error);
    void cameraUpdated(const CameraRecord& camera);
    void cameraUpdateFailed(const QString& error);

private slots:
    void handleLoginResult(const AuthResult& result);
    void handlePersonsFetched(const QList<PersonRecord>& persons);
    void handlePersonsFetchFailed(const QString& error);
    void handleEventPosted(const EventPayload& event);
    void handleEventPostFailed(const EventPayload& event, const QString& error);
    void handleSyncTick();
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
    void handleCamerasFetched(const QList<CameraRecord>& cameras);
    void handleCamerasFetchFailed(const QString& error);
    void handleCameraCreated(const CameraRecord& camera);
    void handleCameraCreateFailed(const QString& error);
    void handleCameraDeleted(const QString& cameraId);
    void handleCameraDeleteFailed(const QString& error);
    void handleCameraUpdated(const CameraRecord& camera);
    void handleCameraUpdateFailed(const QString& error);

private:
    void requestPersonsRefresh();
    bool ensureAuthenticated();
    bool writeEmbeddingsFile(const QList<EmbeddingRecord>& embeddings, QString* errorOut = nullptr) const;
    bool mirrorEmbeddingsToRuntime(QString* errorOut = nullptr) const;
    void removeRuntimeEmbeddings() const;
    PersonRecord personById(const QString& id) const;
    QString cameraSourceForLocal(int cameraId) const;
    QString cameraIdForStream(const QString& streamUrl) const;
    QString cameraIdForLocal(int cameraId) const;

    SupabaseClient* client = nullptr;
    AIProcessor* aiProcessor = nullptr;
    CameraManager* cameraManager = nullptr;
    QList<PersonRecord> personsCache;
    QList<EmbeddingRecord> embeddingsCache;
    QList<CameraRecord> camerasCache;
    QHash<QString, PersonRecord> personIndex;
    QHash<QString, PersonRecord> personsById;

    QString email;
    QString password;
    QUrl serverUrl{ QStringLiteral("https://myserver-tc2d.onrender.com") };
    QString configPath;
    QString embeddingsPath;
    QString runtimeEmbeddingsPath;
    int syncIntervalMs = 5000;
    bool configLoaded = false;
    bool loginInProgress = false;
    bool personsRequestActive = false;
    bool embeddingsRequestActive = false;
    bool camerasRequestActive = false;
    bool manualCredentialsProvided = false;
};

#endif // SERVERSYNCMANAGER_H
