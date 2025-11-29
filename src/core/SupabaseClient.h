#ifndef SUPABASECLIENT_H
#define SUPABASECLIENT_H

/**
 * @file SupabaseClient.h
 * @brief Minimal REST client for Drogon/Supabase-like authentication and CRUD.
 *
 * Handles login, person CRUD, alert/event posting, embedding upload, and avatar upload
 * against a configurable base URL. Provides Qt signals for async responses.
 *
 * @example
 * SupabaseClient client;
 * client.login("user@example.com", "secret");
 * connect(&client, &SupabaseClient::loginFinished, [](const AuthResult& r){ /* ... */ });
 */

#include "ServerTypes.h"
#include <QDateTime>
#include <QObject>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

struct AuthResult {
    bool success = false;
    QString message;
    QString token;
    QString userId;
    QDateTime expiresAt;
};

struct LoginSession {
    QString email;
    QString password;
    AuthResult auth;

    /**
     * @brief Indicates whether a valid token is present.
     * @return bool True when authentication succeeded and token is non-empty.
     */
    bool isValid() const { return auth.success && !auth.token.isEmpty(); }
};

/**
 * @brief HTTP client encapsulating auth, person, event, alert, and embedding endpoints.
 */
class SupabaseClient : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the client with default base URL and auth endpoint.
     * @param parent Optional QObject parent.
     * @throws None
     * @example SupabaseClient client;
     */
    explicit SupabaseClient(QObject* parent = nullptr);

    /**
     * @brief Sets the server base URL for all requests.
     * @param url Base URL (e.g., https://example.com).
     * @return void
     * @throws None
     * @example client.setBaseUrl(QUrl("https://api.example.com"));
     */
    void setBaseUrl(const QUrl& url) { baseUrl = url; }
    /**
     * @brief Overrides the authentication endpoint path.
     * @param path Endpoint path (e.g., "/auth/login").
     * @return void
     * @throws None
     * @example client.setAuthEndpoint("/auth/login");
     */
    void setAuthEndpoint(const QString& path); // e.g. "/auth/login"

    /**
     * @brief Performs an async login with email/password.
     * @param email User email.
     * @param password User password.
     * @return void
     * @throws None
     * @example client.login("user@example.com", "secret");
     */
    void login(const QString& email, const QString& password);
    /**
     * @brief Checks whether a valid auth token is cached.
     * @return bool True if token exists and not expired.
     * @throws None
     */
    bool isAuthenticated() const;
    /**
     * @brief Returns the cached bearer token.
     * @return QString Token string.
     * @throws None
     */
    QString sessionToken() const { return authToken; }
    /**
     * @brief Applies an external session token with expiry.
     * @param token Bearer token.
     * @param expiresAt Expiration time.
     * @return void
     * @throws None
     * @example client.applySession(token, expiry);
     */
    void applySession(const QString& token, const QDateTime& expiresAt);
    /**
     * @brief Clears authentication state.
     * @return void
     * @throws None
     * @example client.clearSession();
     */
    void clearSession();

    /**
     * @brief Fetches person list asynchronously.
     * @return void
     * @throws None
     * @example client.fetchPersons();
     */
    void fetchPersons();
    /**
     * @brief Posts a single detection event.
     * @param event Event payload.
     * @return void
     * @throws None
     */
    void postEvent(const EventPayload& event);
    /**
     * @brief Posts a batch of detection events.
     * @param events List of events.
     * @return void
     * @throws None
     */
    void postEvents(const QList<EventPayload>& events);
    /**
     * @brief Posts an alert message with severity.
     * @param alertType Alert type/category.
     * @param message Message body.
     * @param severity Severity string, default "medium".
     * @return void
     * @throws None
     */
    void postAlert(const QString& alertType, const QString& message, const QString& severity = QStringLiteral("medium"));
    /**
     * @brief Creates a person record.
     * @param name Person name.
     * @param role Role/department.
     * @param authorized Authorization flag.
     * @param imageUrl Optional avatar URL.
     * @return void
     * @throws None
     */
    void createPerson(const QString& name, const QString& role, bool authorized, const QString& imageUrl = QString());
    /**
     * @brief Updates fields on a person record.
     * @param personId Person identifier.
     * @param fields JSON object of fields to patch.
     * @return void
     * @throws None
     */
    void updatePerson(const QString& personId, const QJsonObject& fields);
    /**
     * @brief Deletes a person by id.
     * @param personId Person identifier.
     * @return void
     * @throws None
     */
    void deletePerson(const QString& personId);
    /**
     * @brief Fetches embedding records.
     * @return void
     * @throws None
     */
    void fetchEmbeddings();
    /**
     * @brief Posts an embedding vector for a person.
     * @param personId Person identifier.
     * @param modelName Embedding model name.
     * @param vector Embedding vector.
     * @return void
     * @throws None
     */
    void postEmbedding(const QString& personId, const QString& modelName, const QVector<float>& vector);
    /**
     * @brief Uploads an avatar image for a person.
     * @param personId Person identifier.
     * @param image Avatar image.
     * @return void
     * @throws None
     */
    void uploadPersonAvatar(const QString& personId, const QImage& image);

signals:
    void loginFinished(const AuthResult& result);
    void personsFetched(const QList<PersonRecord>& persons);
    void personsFetchFailed(const QString& error);
    void eventPosted(const EventPayload& event);
    void eventPostFailed(const EventPayload& event, const QString& error);
    void alertPosted();
    void alertPostFailed(const QString& error);
    void personCreated(const PersonRecord& person);
    void personCreateFailed(const QString& error);
    void personUpdated(const PersonRecord& person);
    void personUpdateFailed(const QString& error);
    void personDeleted(const QString& personId);
    void personDeleteFailed(const QString& error);
    void embeddingsFetched(const QList<EmbeddingRecord>& embeddings);
    void embeddingsFetchFailed(const QString& error);
    void embeddingPosted();
    void embeddingPostFailed(const QString& error);
    void avatarUploaded();
    void avatarUploadFailed(const QString& error);

private:
    void handleLoginReply(QNetworkReply* reply);
    void handlePersonsReply(QNetworkReply* reply);
    void handleEventReply(QNetworkReply* reply, const EventPayload& event);
    void handleAlertReply(QNetworkReply* reply);
    void handlePersonReply(QNetworkReply* reply);
    void handlePersonUpdateReply(QNetworkReply* reply);
    void handlePersonDeleteReply(QNetworkReply* reply, const QString& personId);
    void handleEmbeddingsReply(QNetworkReply* reply);
    void handleEmbeddingReply(QNetworkReply* reply);
    QNetworkRequest authorizedRequest(const QString& path, bool jsonContent = true) const;

    QNetworkAccessManager network;
    QUrl baseUrl{ QStringLiteral("https://myserver-tc2d.onrender.com") };
    QString authEndpoint{ QStringLiteral("/auth/login") };
    QString authToken;
    QDateTime tokenExpiresAt;
};

#endif // SUPABASECLIENT_H
