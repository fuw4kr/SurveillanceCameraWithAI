#ifndef SUPABASECLIENT_H
#define SUPABASECLIENT_H

#include "ServerTypes.h"
#include <QDateTime>
#include <QObject>
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

    bool isValid() const { return auth.success && !auth.token.isEmpty(); }
};

/**
 * @brief Minimal REST client for Drogon/Supabase-like auth endpoints.
 * Default base URL: https://myserver-tc2d.onrender.com, endpoint: /auth/login.
 */
class SupabaseClient : public QObject
{
    Q_OBJECT
public:
    explicit SupabaseClient(QObject* parent = nullptr);

    void setBaseUrl(const QUrl& url) { baseUrl = url; }
    void setAuthEndpoint(const QString& path); // e.g. "/auth/login"

    void login(const QString& email, const QString& password);
    bool isAuthenticated() const;
    QString sessionToken() const { return authToken; }
    void applySession(const QString& token, const QDateTime& expiresAt);
    void clearSession();

    void fetchPersons();
    void postEvent(const EventPayload& event);
    void postEvents(const QList<EventPayload>& events);

signals:
    void loginFinished(const AuthResult& result);
    void personsFetched(const QList<PersonRecord>& persons);
    void personsFetchFailed(const QString& error);
    void eventPosted(const EventPayload& event);
    void eventPostFailed(const EventPayload& event, const QString& error);

private:
    void handleLoginReply(QNetworkReply* reply);
    void handlePersonsReply(QNetworkReply* reply);
    void handleEventReply(QNetworkReply* reply, const EventPayload& event);
    QNetworkRequest authorizedRequest(const QString& path) const;

    QNetworkAccessManager network;
    QUrl baseUrl{ QStringLiteral("https://myserver-tc2d.onrender.com") };
    QString authEndpoint{ QStringLiteral("/auth/login") };
    QString authToken;
    QDateTime tokenExpiresAt;
};

#endif // SUPABASECLIENT_H
