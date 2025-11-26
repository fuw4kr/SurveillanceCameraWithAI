#include "SupabaseClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <memory>

SupabaseClient::SupabaseClient(QObject* parent)
    : QObject(parent)
{
}

void SupabaseClient::setAuthEndpoint(const QString& path)
{
    authEndpoint = path.startsWith('/') ? path : QStringLiteral("/") + path;
}

void SupabaseClient::login(const QString& email, const QString& password)
{
    QUrl url = baseUrl.resolved(QUrl(authEndpoint));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject payload;
    payload["email"] = email;
    payload["password"] = password;

    QNetworkReply* reply = network.post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, &SupabaseClient::handleLoginReply);
}

void SupabaseClient::handleLoginReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    AuthResult result;

    if (reply->error() != QNetworkReply::NoError) {
        result.message = reply->errorString();
        emit loginFinished(result);
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        result.token = obj.value(QStringLiteral("token")).toString();
        result.message = obj.value(QStringLiteral("message")).toString();
    }

    result.success = !result.token.isEmpty();
    if (!result.success && result.message.isEmpty())
        result.message = QStringLiteral("Authentication failed");

    emit loginFinished(result);
}
