#ifndef SUPABASECLIENT_H
#define SUPABASECLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

struct AuthResult {
    bool success = false;
    QString message;
    QString token;
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

signals:
    void loginFinished(const AuthResult& result);

private slots:
    void handleLoginReply();

private:
    QNetworkAccessManager network;
    QUrl baseUrl{ QStringLiteral("https://myserver-tc2d.onrender.com") };
    QString authEndpoint{ QStringLiteral("/auth/login") };
};

#endif // SUPABASECLIENT_H
