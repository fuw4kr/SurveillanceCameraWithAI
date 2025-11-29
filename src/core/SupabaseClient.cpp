#include "SupabaseClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>

namespace {
QString ensureLeadingSlash(const QString& path)
{
    if (path.isEmpty())
        return QStringLiteral("/");
    return path.startsWith('/') ? path : QStringLiteral("/") + path;
}

QDateTime parseDateTime(const QJsonValue& value)
{
    if (!value.isString())
        return {};
    const QString text = value.toString();
    QDateTime dt = QDateTime::fromString(text, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(text, Qt::ISODate);
    if (dt.isValid() && dt.timeSpec() == Qt::LocalTime)
        dt = dt.toUTC();
    return dt;
}
}

SupabaseClient::SupabaseClient(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<QList<PersonRecord>>("QList<PersonRecord>");
    qRegisterMetaType<EventPayload>("EventPayload");
}

void SupabaseClient::setAuthEndpoint(const QString& path)
{
    authEndpoint = ensureLeadingSlash(path);
}

void SupabaseClient::login(const QString& email, const QString& password)
{
    qInfo() << "[Supabase] Login request started for" << email;
    QUrl url = baseUrl.resolved(QUrl(authEndpoint));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject payload;
    payload.insert(QStringLiteral("email"), email);
    payload.insert(QStringLiteral("password"), password);

    QNetworkReply* reply = network.post(req, QJsonDocument(payload).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleLoginReply(reply);
    });
}

bool SupabaseClient::isAuthenticated() const
{
    if (authToken.isEmpty())
        return false;
    if (!tokenExpiresAt.isValid())
        return true;
    return tokenExpiresAt > QDateTime::currentDateTimeUtc().addSecs(-30);
}

void SupabaseClient::applySession(const QString& token, const QDateTime& expiresAt)
{
    authToken = token;
    tokenExpiresAt = expiresAt;
}

void SupabaseClient::clearSession()
{
    authToken.clear();
    tokenExpiresAt = {};
}

void SupabaseClient::fetchPersons()
{
    qInfo() << "[Supabase] Fetching persons list";
    if (!isAuthenticated()) {
        emit personsFetchFailed(QStringLiteral("Not authenticated"));
        return;
    }

    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/persons"));
    QNetworkReply* reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handlePersonsReply(reply);
    });
}

void SupabaseClient::postEvent(const EventPayload& event)
{
    if (!isAuthenticated()) {
        emit eventPostFailed(event, QStringLiteral("Not authenticated"));
        return;
    }
    qInfo().noquote() << "[Supabase] Posting event" << event.eventType << "camera:" << event.cameraLabel << "confidence:" << event.confidence;
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/events"));
    QJsonObject payload;
    const QString eventType = event.eventType.isEmpty()
        ? QStringLiteral("Detection")
        : event.eventType;
    payload.insert(QStringLiteral("event_type"), eventType);
    if (event.confidence > 0.0f)
        payload.insert(QStringLiteral("confidence"), event.confidence);
    if (!event.timestamp.isNull())
        payload.insert(QStringLiteral("timestamp"), event.timestamp.toUTC().toString(Qt::ISODateWithMs));

    QNetworkReply* reply = network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, event]() {
        handleEventReply(reply, event);
    });
}

void SupabaseClient::postEvents(const QList<EventPayload>& events)
{
    for (const auto& ev : events)
        postEvent(ev);
}

void SupabaseClient::handleLoginReply(QNetworkReply* reply)
{
    AuthResult result;
    if (!reply) {
        emit loginFinished(result);
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        result.message = reply->errorString();
        emit loginFinished(result);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        const QJsonObject obj = doc.object();
        result.token = obj.value(QStringLiteral("token")).toString();
        result.userId = obj.value(QStringLiteral("userId")).toString();
        result.message = obj.value(QStringLiteral("message")).toString();
        result.expiresAt = parseDateTime(obj.value(QStringLiteral("expiresAt")));
    }
    result.success = !result.token.isEmpty();
    if (!result.success && result.message.isEmpty())
        result.message = QStringLiteral("Authentication failed");

    if (result.success)
        applySession(result.token, result.expiresAt);
    else
        clearSession();

    if (result.success)
        qInfo() << "[Supabase] Login successful, token expires at" << result.expiresAt.toString(Qt::ISODate);
    else
        qWarning() << "[Supabase] Login failed:" << result.message;
    emit loginFinished(result);
}

void SupabaseClient::handlePersonsReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Supabase] Fetch persons failed:" << reply->errorString();
        emit personsFetchFailed(reply->errorString());
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit personsFetchFailed(QStringLiteral("Invalid payload"));
        return;
    }

    QList<PersonRecord> persons;
    for (const auto& value : doc.array()) {
        if (!value.isObject())
            continue;
        const QJsonObject obj = value.toObject();
        PersonRecord record;
        record.id = obj.value(QStringLiteral("id")).toString();
        record.name = obj.value(QStringLiteral("name")).toString();
        record.role = obj.value(QStringLiteral("role")).toString();
        record.imageUrl = obj.value(QStringLiteral("image_url")).toString();
        record.authorized = obj.value(QStringLiteral("authorized")).toBool(false);
        record.registeredAt = parseDateTime(obj.value(QStringLiteral("registered_at")));
        record.lastSeen = parseDateTime(obj.value(QStringLiteral("last_seen")));
        persons.append(record);
    }

    qInfo() << "[Supabase] Received" << persons.size() << "remote persons";
    emit personsFetched(persons);
}

void SupabaseClient::handleEventReply(QNetworkReply* reply, const EventPayload& event)
{
    if (!reply) {
        emit eventPostFailed(event, QStringLiteral("Invalid reply"));
        return;
    }
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Supabase] Event post failed:" << reply->errorString();
        emit eventPostFailed(event, reply->errorString());
        return;
    }
    qInfo().noquote() << "[Supabase] Event posted successfully:" << event.eventType;
    emit eventPosted(event);
}

QNetworkRequest SupabaseClient::authorizedRequest(const QString& path) const
{
    const QUrl url = baseUrl.resolved(QUrl(ensureLeadingSlash(path)));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!authToken.isEmpty())
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + authToken.toUtf8());
    return req;
}
