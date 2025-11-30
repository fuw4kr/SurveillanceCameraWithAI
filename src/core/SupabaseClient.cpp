/**
 * @file SupabaseClient.cpp
 * @brief Implements REST interactions for authentication, CRUD, alerts, and embeddings.
 */
#include "SupabaseClient.h"

#include <QBuffer>
#include <QHttpMultiPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QStringList>
#include <QVariant>
#include <QSet>

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

PersonRecord parsePersonRecord(const QJsonObject& obj)
{
    PersonRecord record;
    record.id = obj.value(QStringLiteral("id")).toString();
    record.name = obj.value(QStringLiteral("name")).toString();
    record.role = obj.value(QStringLiteral("role")).toString();
    record.imageUrl = obj.value(QStringLiteral("image_url")).toString();
    record.authorized = obj.value(QStringLiteral("authorized")).toBool(false);
    record.registeredAt = parseDateTime(obj.value(QStringLiteral("registered_at")));
    record.lastSeen = parseDateTime(obj.value(QStringLiteral("last_seen")));
    return record;
}

EmbeddingRecord parseEmbeddingRecord(const QJsonObject& obj)
{
    EmbeddingRecord record;
    record.id = obj.value(QStringLiteral("id")).toString();
    record.personId = obj.value(QStringLiteral("person_id")).toString();
    record.modelName = obj.value(QStringLiteral("model_name")).toString();
    record.createdAt = parseDateTime(obj.value(QStringLiteral("created_at")));
    QJsonArray arr = obj.value(QStringLiteral("vector")).toArray();
    if (arr.isEmpty())
        arr = obj.value(QStringLiteral("embedding")).toArray();
    for (const auto& v : arr)
        record.vector.append(static_cast<float>(v.toDouble()));
    return record;
}

CameraRecord parseCameraRecord(const QJsonObject& obj)
{
    CameraRecord record;
    record.id = obj.value(QStringLiteral("id")).toString();
    record.name = obj.value(QStringLiteral("name")).toString();
    record.ipAddress = obj.value(QStringLiteral("ip_address")).toString();
    record.location = obj.value(QStringLiteral("location")).toString();
    record.status = obj.value(QStringLiteral("status")).toString();
    record.streamUrl = obj.value(QStringLiteral("stream_url")).toString();
    record.createdAt = parseDateTime(obj.value(QStringLiteral("created_at")));
    return record;
}

QString describeNetworkResponse(int statusCode, const QString& reason, const QString& error, const QByteArray& body, bool includeBody)
{
    QStringList parts;
    if (statusCode > 0)
        parts << QStringLiteral("HTTP %1").arg(statusCode);
    if (!reason.isEmpty())
        parts << reason;
    if (!error.isEmpty())
        parts << QStringLiteral("error: %1").arg(error);
    if (includeBody && !body.isEmpty()) {
        const QString bodyText = QString::fromUtf8(body.left(512)).trimmed();
        if (!bodyText.isEmpty())
            parts << QStringLiteral("body: %1").arg(bodyText);
    }
    if (parts.isEmpty())
        return QStringLiteral("No response details");
    return parts.join(QStringLiteral(" | "));
}
}

SupabaseClient::SupabaseClient(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<QList<PersonRecord>>("QList<PersonRecord>");
    qRegisterMetaType<EventPayload>("EventPayload");
    qRegisterMetaType<QList<EmbeddingRecord>>("QList<EmbeddingRecord>");
    qRegisterMetaType<QList<CameraRecord>>("QList<CameraRecord>");
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
    static const QSet<QString> allowedTypes = {
        QStringLiteral("detect_start"),
        QStringLiteral("detect_end"),
        QStringLiteral("alert")
    };
    const QString type = event.eventType.isEmpty() ? QStringLiteral("alert") : event.eventType;
    if (!allowedTypes.contains(type)) {
        emit eventPostFailed(event, QStringLiteral("Unsupported event type: %1").arg(type));
        return;
    }
    if (event.cameraId.isEmpty()) {
        emit eventPostFailed(event, QStringLiteral("Missing camera id"));
        return;
    }
    if (event.personId.isEmpty()) {
        emit eventPostFailed(event, QStringLiteral("Missing person id"));
        return;
    }
    if (type == QStringLiteral("detect_start") && event.snapshotUrl.isEmpty()) {
        emit eventPostFailed(event, QStringLiteral("Missing snapshot for detect_start event"));
        return;
    }
    const QString cameraRef = event.cameraId;
    qInfo().noquote() << "[Supabase] Posting event" << type << "camera:" << cameraRef << "person:" << event.personId;
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/events"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject payload;
    payload.insert(QStringLiteral("event_type"), type);
    payload.insert(QStringLiteral("camera_id"), event.cameraId);
    payload.insert(QStringLiteral("person_id"), event.personId);
    if (event.confidence > 0.0f)
        payload.insert(QStringLiteral("confidence"), event.confidence);
    const QDateTime timestamp = event.timestamp.isValid() ? event.timestamp.toUTC() : QDateTime::currentDateTimeUtc();
    payload.insert(QStringLiteral("timestamp"), timestamp.toString(Qt::ISODateWithMs));
    if (!event.snapshotUrl.isEmpty())
        payload.insert(QStringLiteral("snapshot_url"), event.snapshotUrl);

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

void SupabaseClient::fetchStatsSummary()
{
    if (!isAuthenticated()) {
        emit statsFetchFailed(QStringLiteral("summary"), QStringLiteral("Not authenticated"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/stats/summary"));
    QNetworkReply* reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleStatsReply(reply, QStringLiteral("summary"));
    });
}

void SupabaseClient::fetchStatsDetectionsByHour()
{
    if (!isAuthenticated()) {
        emit statsFetchFailed(QStringLiteral("detections_by_hour"), QStringLiteral("Not authenticated"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/stats/detections-by-hour"));
    QNetworkReply* reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleStatsReply(reply, QStringLiteral("detections_by_hour"));
    });
}

void SupabaseClient::fetchStatsEvents()
{
    if (!isAuthenticated()) {
        emit statsFetchFailed(QStringLiteral("events"), QStringLiteral("Not authenticated"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/stats/events"));
    QNetworkReply* reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleStatsReply(reply, QStringLiteral("events"));
    });
}

void SupabaseClient::postAlert(const QString& alertType, const QString& message, const QString& severity, const QImage& snapshot)
{
    if (!isAuthenticated()) {
        emit alertPostFailed(QStringLiteral("Not authenticated"));
        return;
    }
    const QString resolvedSeverity = severity.isEmpty() ? QStringLiteral("medium") : severity;
    QByteArray imageBytes;
    if (!snapshot.isNull()) {
        QBuffer buffer(&imageBytes);
        buffer.open(QIODevice::WriteOnly);
        snapshot.save(&buffer, "PNG");
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/alerts"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject payload;
    payload.insert(QStringLiteral("alert_type"), alertType);
    payload.insert(QStringLiteral("message"), message);
    payload.insert(QStringLiteral("severity"), resolvedSeverity);
    if (!imageBytes.isEmpty()) {
        payload.insert(QStringLiteral("snapshot"),
            QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(imageBytes.toBase64())));
    }
    QNetworkReply* reply = network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleAlertReply(reply);
    });
}

void SupabaseClient::createPerson(const QString& name, const QString& role, bool authorized, const QString& imageUrl)
{
    if (!isAuthenticated()) {
        emit personCreateFailed(QStringLiteral("Not authenticated"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/persons"));
    QJsonObject payload;
    payload.insert(QStringLiteral("name"), name);
    payload.insert(QStringLiteral("role"), role);
    payload.insert(QStringLiteral("authorized"), authorized);
    if (!imageUrl.isEmpty())
        payload.insert(QStringLiteral("image_url"), imageUrl);
    QNetworkReply* reply = network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handlePersonReply(reply);
    });
}

void SupabaseClient::updatePerson(const QString& personId, const QJsonObject& fields)
{
    if (!isAuthenticated()) {
        emit personUpdateFailed(QStringLiteral("Not authenticated"));
        return;
    }
    if (personId.isEmpty()) {
        emit personUpdateFailed(QStringLiteral("Missing person id"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/persons/") + personId);
    QNetworkReply* reply = network.sendCustomRequest(req, QByteArrayLiteral("PATCH"),
        QJsonDocument(fields).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handlePersonUpdateReply(reply);
    });
}

void SupabaseClient::deletePerson(const QString& personId)
{
    if (!isAuthenticated()) {
        emit personDeleteFailed(QStringLiteral("Not authenticated"));
        return;
    }
    if (personId.isEmpty()) {
        emit personDeleteFailed(QStringLiteral("Missing person id"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/persons/") + personId);
    QNetworkReply* reply = network.deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, personId]() {
        handlePersonDeleteReply(reply, personId);
    });
}

void SupabaseClient::fetchEmbeddings()
{
    if (!isAuthenticated()) {
        emit embeddingsFetchFailed(QStringLiteral("Not authenticated"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/embeddings"));
    QNetworkReply* reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleEmbeddingsReply(reply);
    });
}

void SupabaseClient::postEmbedding(const QString& personId, const QString& modelName, const QVector<float>& vector)
{
    if (!isAuthenticated()) {
        emit embeddingPostFailed(QStringLiteral("Not authenticated"));
        return;
    }
    if (personId.isEmpty() || vector.isEmpty()) {
        emit embeddingPostFailed(QStringLiteral("Missing embedding payload"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/embeddings"));
    QJsonObject payload;
    payload.insert(QStringLiteral("person_id"), personId);
    payload.insert(QStringLiteral("model_name"), modelName.isEmpty() ? QStringLiteral("FaceNet") : modelName);
    QJsonArray vecArray;
    for (float v : vector)
        vecArray.append(v);
    payload.insert(QStringLiteral("vector"), vecArray);
    payload.insert(QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    QNetworkReply* reply = network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleEmbeddingReply(reply);
    });
}

void SupabaseClient::uploadPersonAvatar(const QString& personId, const QImage& image)
{
    if (!isAuthenticated()) {
        emit avatarUploadFailed(QStringLiteral("Not authenticated"));
        return;
    }
    if (personId.isEmpty() || image.isNull()) {
        emit avatarUploadFailed(QStringLiteral("Missing avatar payload"));
        return;
    }
    QByteArray bufferArray;
    QBuffer buffer(&bufferArray);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");

    QHttpMultiPart* multipart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
        QVariant(QStringLiteral("form-data; name=\"avatar\"; filename=\"face.png\"")));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("image/png")));
    filePart.setBody(bufferArray);
    multipart->append(filePart);

    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/persons/") + personId + QStringLiteral("/avatar"), false);
    QNetworkReply* reply = network.post(req, multipart);
    multipart->setParent(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (!reply)
            return;
        const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        const QVariant reasonAttr = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
        const int statusCode = statusAttr.isValid() ? statusAttr.toInt() : 0;
        const QString reason = reasonAttr.isValid() ? reasonAttr.toString() : QString();
        const QString errorText = reply->errorString();
        const QByteArray body = reply->readAll();
        const bool failed = reply->error() != QNetworkReply::NoError;
        const QString detail = describeNetworkResponse(statusCode, reason, failed ? errorText : QString(), body, failed);
        reply->deleteLater();
        if (failed) {
            qWarning() << "[Supabase]" << "Avatar upload failed:" << detail;
            emit avatarUploadFailed(detail);
            return;
        }
        qInfo() << "[Supabase]" << "Avatar uploaded:" << detail;
        emit avatarUploaded();
    });
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
        persons.append(parsePersonRecord(value.toObject()));
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

void SupabaseClient::handleAlertReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const QVariant reasonAttr = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
    const int statusCode = statusAttr.isValid() ? statusAttr.toInt() : 0;
    const QString reason = reasonAttr.isValid() ? reasonAttr.toString() : QString();
    const QString errorText = reply->errorString();
    const QByteArray body = reply->readAll();
    const bool failed = reply->error() != QNetworkReply::NoError;
    const QString detail = describeNetworkResponse(statusCode, reason, failed ? errorText : QString(), body, failed);
    reply->deleteLater();
    if (failed) {
        qWarning() << "[Supabase]" << "Alert post failed:" << detail;
        emit alertPostFailed(detail);
        return;
    }
    qInfo() << "[Supabase]" << "Alert posted:" << detail;
    emit alertPosted();
}

void SupabaseClient::fetchCameras()
{
    if (!isAuthenticated()) {
        emit camerasFetchFailed(QStringLiteral("Not authenticated"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/cameras"));
    QNetworkReply* reply = network.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleCamerasReply(reply);
    });
}

void SupabaseClient::createCamera(const QString& name, const QString& streamUrl, const QString& ipAddress, const QString& location, const QString& status)
{
    if (!isAuthenticated()) {
        emit cameraCreateFailed(QStringLiteral("Not authenticated"));
        return;
    }
    QJsonObject payload;
    if (!name.isEmpty())
        payload.insert(QStringLiteral("name"), name);
    if (!ipAddress.isEmpty())
        payload.insert(QStringLiteral("ip_address"), ipAddress);
    if (!location.isEmpty())
        payload.insert(QStringLiteral("location"), location);
    if (!status.isEmpty())
        payload.insert(QStringLiteral("status"), status);
    if (!streamUrl.isEmpty())
        payload.insert(QStringLiteral("stream_url"), streamUrl);
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/cameras"));
    QNetworkReply* reply = network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleCameraCreateReply(reply);
    });
}

void SupabaseClient::deleteCamera(const QString& cameraId)
{
    if (!isAuthenticated()) {
        emit cameraDeleteFailed(QStringLiteral("Not authenticated"));
        return;
    }
    if (cameraId.isEmpty()) {
        emit cameraDeleteFailed(QStringLiteral("Missing camera id"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/cameras/") + cameraId, false);
    QNetworkReply* reply = network.deleteResource(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, cameraId]() {
        handleCameraDeleteReply(reply, cameraId);
    });
}

void SupabaseClient::updateCamera(const QString& cameraId, const QJsonObject& fields)
{
    if (!isAuthenticated()) {
        emit cameraUpdateFailed(QStringLiteral("Not authenticated"));
        return;
    }
    if (cameraId.isEmpty() || fields.isEmpty()) {
        emit cameraUpdateFailed(QStringLiteral("Missing camera update payload"));
        return;
    }
    QNetworkRequest req = authorizedRequest(QStringLiteral("/api/cameras/") + cameraId);
    const QByteArray payload = QJsonDocument(fields).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = network.sendCustomRequest(req, QByteArrayLiteral("PATCH"), payload);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleCameraUpdateReply(reply);
    });
}

void SupabaseClient::handlePersonReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit personCreateFailed(reply->errorString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        emit personCreated(parsePersonRecord(doc.object()));
        return;
    }
    if (doc.isArray() && !doc.array().isEmpty() && doc.array().first().isObject()) {
        emit personCreated(parsePersonRecord(doc.array().first().toObject()));
        return;
    }
    emit personCreateFailed(QStringLiteral("Invalid response"));
}

void SupabaseClient::handlePersonUpdateReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit personUpdateFailed(reply->errorString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        emit personUpdated(parsePersonRecord(doc.object()));
    } else {
        emit personUpdateFailed(QStringLiteral("Invalid response"));
    }
}

void SupabaseClient::handlePersonDeleteReply(QNetworkReply* reply, const QString& personId)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit personDeleteFailed(reply->errorString());
        return;
    }
    emit personDeleted(personId);
}

void SupabaseClient::handleEmbeddingsReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit embeddingsFetchFailed(reply->errorString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit embeddingsFetchFailed(QStringLiteral("Invalid payload"));
        return;
    }
    QList<EmbeddingRecord> embeddings;
    for (const auto& value : doc.array()) {
        if (!value.isObject())
            continue;
        embeddings.append(parseEmbeddingRecord(value.toObject()));
    }
    emit embeddingsFetched(embeddings);
}

void SupabaseClient::handleEmbeddingReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    const QVariant statusAttr = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const QVariant reasonAttr = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute);
    const int statusCode = statusAttr.isValid() ? statusAttr.toInt() : 0;
    const QString reason = reasonAttr.isValid() ? reasonAttr.toString() : QString();
    const QString errorText = reply->errorString();
    const QByteArray body = reply->readAll();
    const bool failed = reply->error() != QNetworkReply::NoError;
    const QString detail = describeNetworkResponse(statusCode, reason, failed ? errorText : QString(), body, failed);
    reply->deleteLater();
    if (failed) {
        qWarning() << "[Supabase]" << "Embedding post failed:" << detail;
        emit embeddingPostFailed(detail);
        return;
    }
    qInfo() << "[Supabase]" << "Embedding posted:" << detail;
    emit embeddingPosted();
}

void SupabaseClient::handleCamerasReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit camerasFetchFailed(reply->errorString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isArray()) {
        emit camerasFetchFailed(QStringLiteral("Invalid payload"));
        return;
    }
    QList<CameraRecord> cameras;
    for (const auto& value : doc.array()) {
        if (!value.isObject())
            continue;
        cameras.append(parseCameraRecord(value.toObject()));
    }
    emit camerasFetched(cameras);
}

void SupabaseClient::handleCameraCreateReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit cameraCreateFailed(reply->errorString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        emit cameraCreated(parseCameraRecord(doc.object()));
        return;
    }
    if (doc.isArray() && !doc.array().isEmpty() && doc.array().first().isObject()) {
        emit cameraCreated(parseCameraRecord(doc.array().first().toObject()));
        return;
    }
    emit cameraCreateFailed(QStringLiteral("Invalid response"));
}

void SupabaseClient::handleCameraDeleteReply(QNetworkReply* reply, const QString& cameraId)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit cameraDeleteFailed(reply->errorString());
        return;
    }
    emit cameraDeleted(cameraId);
}

void SupabaseClient::handleCameraUpdateReply(QNetworkReply* reply)
{
    if (!reply)
        return;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        emit cameraUpdateFailed(reply->errorString());
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        emit cameraUpdated(parseCameraRecord(doc.object()));
        return;
    }
    if (doc.isArray() && !doc.array().isEmpty() && doc.array().first().isObject()) {
        emit cameraUpdated(parseCameraRecord(doc.array().first().toObject()));
        return;
    }
    emit cameraUpdateFailed(QStringLiteral("Invalid response"));
}

void SupabaseClient::handleStatsReply(QNetworkReply* reply, const QString& key)
{
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString reason = reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        const QString error = describeNetworkResponse(statusCode, reason, reply->errorString(), body, true);
        emit statsFetchFailed(key, error);
        reply->deleteLater();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        emit statsFetchFailed(key, QStringLiteral("Parse error: %1").arg(parseError.errorString()));
        reply->deleteLater();
        return;
    }

    if (key == QStringLiteral("summary")) {
        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject();
        emit statsSummaryReceived(obj);
    } else if (key == QStringLiteral("detections_by_hour")) {
        QJsonArray arr = doc.isArray() ? doc.array() : doc.object().value(QStringLiteral("data")).toArray();
        if (arr.isEmpty())
            arr = doc.object().value(QStringLiteral("detections")).toArray();
        emit statsDetectionsByHourReceived(arr);
    } else if (key == QStringLiteral("events")) {
        QJsonArray arr = doc.isArray() ? doc.array() : doc.object().value(QStringLiteral("events")).toArray();
        if (arr.isEmpty())
            arr = doc.object().value(QStringLiteral("data")).toArray();
        emit statsEventsReceived(arr);
    }

    reply->deleteLater();
}


QNetworkRequest SupabaseClient::authorizedRequest(const QString& path, bool jsonContent) const
{
    const QUrl url = baseUrl.resolved(QUrl(ensureLeadingSlash(path)));
    QNetworkRequest req(url);
    if (jsonContent)
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!authToken.isEmpty())
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + authToken.toUtf8());
    return req;
}
