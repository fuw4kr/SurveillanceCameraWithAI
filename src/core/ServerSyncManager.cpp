#include "ServerSyncManager.h"
#include "CameraManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtGlobal>
#include <algorithm>

namespace {
QString defaultConfigPath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/config/server.json");
}

QString readString(const QJsonObject& obj, const char* key, const QString& fallback = QString())
{
    const auto value = obj.value(QLatin1String(key));
    return value.isString() ? value.toString() : fallback;
}

int readInt(const QJsonObject& obj, const char* key, int fallback)
{
    const auto value = obj.value(QLatin1String(key));
    if (!value.isDouble())
        return fallback;
    return value.toInt(fallback);
}
}

ServerSyncManager::ServerSyncManager(QObject* parent)
    : QObject(parent)
{
    client = new SupabaseClient(this);
    connect(client, &SupabaseClient::loginFinished, this, &ServerSyncManager::handleLoginResult);
    connect(client, &SupabaseClient::personsFetched, this, &ServerSyncManager::handlePersonsFetched);
    connect(client, &SupabaseClient::personsFetchFailed, this, &ServerSyncManager::handlePersonsFetchFailed);
    connect(client, &SupabaseClient::eventPosted, this, &ServerSyncManager::handleEventPosted);
    connect(client, &SupabaseClient::eventPostFailed, this, &ServerSyncManager::handleEventPostFailed);
    connect(client, &SupabaseClient::alertPosted, this, &ServerSyncManager::handleAlertPosted);
    connect(client, &SupabaseClient::alertPostFailed, this, &ServerSyncManager::handleAlertFailed);
    connect(client, &SupabaseClient::personCreated, this, &ServerSyncManager::handlePersonCreated);
    connect(client, &SupabaseClient::personCreateFailed, this, &ServerSyncManager::handlePersonFailed);
    connect(client, &SupabaseClient::personUpdated, this, &ServerSyncManager::handlePersonUpdated);
    connect(client, &SupabaseClient::personUpdateFailed, this, &ServerSyncManager::handlePersonUpdateFailed);
    connect(client, &SupabaseClient::personDeleted, this, &ServerSyncManager::handlePersonDeleted);
    connect(client, &SupabaseClient::personDeleteFailed, this, &ServerSyncManager::handlePersonDeleteFailed);
    connect(client, &SupabaseClient::embeddingsFetched, this, &ServerSyncManager::handleEmbeddingsFetched);
    connect(client, &SupabaseClient::embeddingsFetchFailed, this, &ServerSyncManager::handleEmbeddingsFetchFailed);
    connect(client, &SupabaseClient::embeddingPosted, this, &ServerSyncManager::handleEmbeddingPosted);
    connect(client, &SupabaseClient::embeddingPostFailed, this, &ServerSyncManager::handleEmbeddingFailed);
    connect(client, &SupabaseClient::avatarUploaded, this, &ServerSyncManager::handleAvatarUploaded);
    connect(client, &SupabaseClient::avatarUploadFailed, this, &ServerSyncManager::handleAvatarUploadFailed);
    connect(client, &SupabaseClient::camerasFetched, this, &ServerSyncManager::handleCamerasFetched);
    connect(client, &SupabaseClient::camerasFetchFailed, this, &ServerSyncManager::handleCamerasFetchFailed);
    connect(client, &SupabaseClient::cameraCreated, this, &ServerSyncManager::handleCameraCreated);
    connect(client, &SupabaseClient::cameraCreateFailed, this, &ServerSyncManager::handleCameraCreateFailed);
    connect(client, &SupabaseClient::cameraDeleted, this, &ServerSyncManager::handleCameraDeleted);
    connect(client, &SupabaseClient::cameraDeleteFailed, this, &ServerSyncManager::handleCameraDeleteFailed);
    connect(client, &SupabaseClient::cameraUpdated, this, &ServerSyncManager::handleCameraUpdated);
    connect(client, &SupabaseClient::cameraUpdateFailed, this, &ServerSyncManager::handleCameraUpdateFailed);

    syncTimer.setSingleShot(false);
    syncTimer.setInterval(syncIntervalMs);
    connect(&syncTimer, &QTimer::timeout, this, &ServerSyncManager::handleSyncTick);

    embeddingsPath = QCoreApplication::applicationDirPath() + QStringLiteral("/config/embeddings_remote.json");
}

void ServerSyncManager::setCredentials(const QString& emailValue, const QString& passwordValue)
{
    email = emailValue.trimmed();
    password = passwordValue;
    manualCredentialsProvided = !(email.isEmpty() && password.isEmpty());
    if (manualCredentialsProvided)
        qInfo() << "[ServerSync]" << "Credentials updated for" << email;
}

void ServerSyncManager::applySessionToken(const QString& token, const QDateTime& expiresAt)
{
    if (client)
        client->applySession(token, expiresAt);
}

void ServerSyncManager::clearSession()
{
    if (client)
        client->clearSession();
}

void ServerSyncManager::sendUnknownAlert(const QString& cameraLabel, const QString& note)
{
    const QString alertType = QStringLiteral("unknown_face");
    QString message = note;
    if (message.isEmpty())
        message = tr("Unknown face detected on %1").arg(cameraLabel);
    if (!client->isAuthenticated() && !ensureAuthenticated()) {
        emit alertSubmissionFailed(tr("Not authenticated"));
        return;
    }
    qInfo() << "[ServerSync]" << "Submitting unknown-face alert for" << cameraLabel << ":" << message;
    client->postAlert(alertType, message, QStringLiteral("high"));
}

void ServerSyncManager::submitPersonRecord(const QString& name, const QString& role, bool authorized)
{
    if (!client->isAuthenticated() && !ensureAuthenticated()) {
        emit personSubmissionFailed(tr("Not authenticated"));
        return;
    }
    qInfo() << "[ServerSync]" << "Submitting person record:" << name << role << "auth" << authorized;
    client->createPerson(name, role, authorized);
}

QString ServerSyncManager::personIdForName(const QString& name) const
{
    if (name.isEmpty())
        return {};
    const QString key = name.toLower();
    return personIndex.contains(key) ? personIndex.value(key).id : QString();
}

void ServerSyncManager::submitCameraRecord(const QString& name, const QString& streamUrl, const QString& ipAddress, const QString& location)
{
    if (streamUrl.isEmpty()) {
        emit errorMessage(tr("Camera stream URL is required."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    const QString label = name.isEmpty() ? streamUrl : name;
    qInfo() << "[ServerSync]" << "Submitting camera record:" << label << streamUrl;
    emit statusMessage(tr("Registering camera \"%1\"").arg(label));
    client->createCamera(label, streamUrl, ipAddress, location);
}

void ServerSyncManager::sendDetectionStatus(const QString& personId, int cameraId, bool active, const QDateTime& timestamp)
{
    if (personId.isEmpty())
        return;
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    EventPayload payload;
    payload.eventType = active ? QStringLiteral("face_detected") : QStringLiteral("face_lost");
    payload.personId = personId;
    const QString source = cameraSourceForLocal(cameraId);
    payload.cameraLabel = source.isEmpty() ? QStringLiteral("Camera %1").arg(cameraId) : source;
    payload.cameraId = cameraIdForStream(source);
    payload.timestamp = timestamp.isValid() ? timestamp : QDateTime::currentDateTimeUtc();
    client->postEvent(payload);
    qInfo() << "[ServerSync]" << payload.eventType << "reported for person" << personId << "camera" << (payload.cameraId.isEmpty() ? QString::number(cameraId) : payload.cameraId);
}

void ServerSyncManager::renamePerson(const QString& personId, const QString& newName)
{
    if (personId.isEmpty() || newName.trimmed().isEmpty()) {
        emit errorMessage(tr("Select a valid person to rename."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    QJsonObject fields;
    fields.insert(QStringLiteral("name"), newName.trimmed());
    client->updatePerson(personId, fields);
    emit statusMessage(tr("Updating \"%1\"...").arg(newName));
}

void ServerSyncManager::updatePersonRole(const QString& personId, const QString& newRole)
{
    if (personId.isEmpty()) {
        emit errorMessage(tr("Select a person to edit role."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    QJsonObject fields;
    fields.insert(QStringLiteral("role"), newRole.trimmed());
    client->updatePerson(personId, fields);
    emit statusMessage(newRole.trimmed().isEmpty() ? tr("Clearing role...") : tr("Updating role to \"%1\"...").arg(newRole.trimmed()));
}

void ServerSyncManager::deletePerson(const QString& personId)
{
    if (personId.isEmpty()) {
        emit errorMessage(tr("Select a person to delete."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    client->deletePerson(personId);
    emit statusMessage(tr("Deleting person..."));
}

void ServerSyncManager::uploadEmbedding(const QString& personId, const QString& modelName, const QVector<float>& vector)
{
    if (personId.isEmpty() || vector.isEmpty()) {
        emit errorMessage(tr("Missing embedding data."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    client->postEmbedding(personId, modelName, vector);
}

void ServerSyncManager::uploadPersonAvatar(const QString& personId, const QImage& image)
{
    if (personId.isEmpty() || image.isNull()) {
        emit errorMessage(tr("Missing avatar image."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    client->uploadPersonAvatar(personId, image);
}

bool ServerSyncManager::loadConfig(const QString& path)
{
    QString resolved = path.isEmpty() ? defaultConfigPath() : path;
    configPath = resolved;

    QFile file(resolved);
    if (file.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                const auto obj = doc.object();
                const QString urlString = readString(obj, "base_url", serverUrl.toString());
                QUrl parsedUrl(urlString);
                if (parsedUrl.isValid())
                    serverUrl = parsedUrl;
                if (!manualCredentialsProvided) {
                    const QString fileEmail = readString(obj, "email", QString());
                    const QString filePassword = readString(obj, "password", QString());
                    if (!fileEmail.isEmpty())
                        email = fileEmail;
                    if (!filePassword.isEmpty())
                        password = filePassword;
                }
                syncIntervalMs = readInt(obj, "sync_interval_ms", syncIntervalMs);
            }
        } else {
            qWarning() << "Unable to open server config" << resolved << file.errorString();
        }
    } else {
        qWarning() << "Server config not found, using defaults:" << resolved;
    }

    const QString envUrl = qEnvironmentVariable("SURV_SERVER_URL");
    if (!envUrl.isEmpty()) {
        QUrl parsed(envUrl);
        if (parsed.isValid())
            serverUrl = parsed;
    }
    if (!manualCredentialsProvided) {
        const QString envEmail = qEnvironmentVariable("SURV_EMAIL");
        if (!envEmail.isEmpty())
            email = envEmail;
        const QString envPassword = qEnvironmentVariable("SURV_PASSWORD");
        if (!envPassword.isEmpty())
            password = envPassword;
    }
    const QString envInterval = qEnvironmentVariable("SURV_SYNC_MS");
    if (!envInterval.isEmpty()) {
        bool ok = false;
        const int value = envInterval.toInt(&ok);
        if (ok)
            syncIntervalMs = value;
    }

    if (!serverUrl.isValid())
        serverUrl = QUrl(QStringLiteral("https://myserver-tc2d.onrender.com"));

    syncIntervalMs = std::clamp(syncIntervalMs, 1000, 60000);
    syncTimer.setInterval(syncIntervalMs);
    client->setBaseUrl(serverUrl);
    configLoaded = true;
    qInfo() << "[ServerSync]" << "Configuration ready. Base URL:" << serverUrl.toString() << "interval:" << syncIntervalMs << "ms";
    return true;
}

void ServerSyncManager::setAiProcessor(AIProcessor* processor)
{
    if (aiProcessor == processor)
        return;
    if (aiProcessor)
        disconnect(aiProcessor, nullptr, this, nullptr);
    aiProcessor = processor;
}

void ServerSyncManager::setCameraManager(CameraManager* manager)
{
    cameraManager = manager;
}

void ServerSyncManager::start()
{
    if (!configLoaded)
        loadConfig(QString());
    handleSyncTick();
    qInfo() << "[ServerSync]" << "Manual synchronization ready";
}

void ServerSyncManager::requestImmediatePersonsRefresh()
{
    personsRequestActive = false;
    requestPersonsRefresh();
}

void ServerSyncManager::deleteCameraRecord(const QString& cameraId)
{
    if (cameraId.isEmpty()) {
        emit errorMessage(tr("Select a camera to delete."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    qInfo() << "[ServerSync]" << "Deleting camera" << cameraId;
    emit statusMessage(tr("Deleting camera..."));
    client->deleteCamera(cameraId);
}

void ServerSyncManager::updateCameraStatus(const QString& cameraId, const QString& status)
{
    const QString trimmedStatus = status.trimmed();
    if (cameraId.isEmpty() || trimmedStatus.isEmpty()) {
        emit errorMessage(tr("Missing camera status update parameters."));
        return;
    }
    if (!client->isAuthenticated() && !ensureAuthenticated())
        return;
    QJsonObject fields;
    fields.insert(QStringLiteral("status"), trimmedStatus);
    qInfo() << "[ServerSync]" << "Updating camera status" << cameraId << trimmedStatus;
    emit statusMessage(tr("Updating camera status to \"%1\"").arg(trimmedStatus));
    client->updateCamera(cameraId, fields);
}

void ServerSyncManager::requestImmediateCamerasRefresh()
{
    camerasRequestActive = false;
    requestCamerasRefresh();
}

void ServerSyncManager::handleLoginResult(const AuthResult& result)
{
    loginInProgress = false;
    if (!result.success) {
        qWarning() << "[ServerSync]" << "Login failed:" << result.message;
        emit errorMessage(tr("Login failed: %1").arg(result.message));
        return;
    }
    qInfo() << "[ServerSync]" << "Authenticated. Token expires at" << result.expiresAt.toString(Qt::ISODate);
    emit statusMessage(tr("Authenticated to %1").arg(serverUrl.host()));
    requestPersonsRefresh();
    requestCamerasRefresh();
    flushEventQueue();
}

void ServerSyncManager::handlePersonsFetched(const QList<PersonRecord>& persons)
{
    personsCache = persons;
    personIndex.clear();
    personsById.clear();
    for (const auto& person : persons) {
        if (!person.name.isEmpty())
            personIndex.insert(person.name.toLower(), person);
        personsById.insert(person.id, person);
    }
    personsRequestActive = false;
    emit personsUpdated(persons);
    emit statusMessage(tr("Synced %1 person records").arg(persons.size()));
    qInfo() << "[ServerSync]" << "Persons sync completed:" << persons.size() << "records";
    requestEmbeddingsRefresh();
}

void ServerSyncManager::handlePersonsFetchFailed(const QString& error)
{
    personsRequestActive = false;
    emit errorMessage(tr("Failed to fetch persons: %1").arg(error));
    qWarning() << "[ServerSync]" << "Persons sync failed:" << error;
}

void ServerSyncManager::handleEventPosted(const EventPayload& event)
{
    if (!eventQueue.isEmpty())
        eventQueue.dequeue();
    eventRequestActive = false;
    emit statusMessage(tr("Sent event: %1").arg(event.eventType));
    flushEventQueue();
    qInfo().noquote() << "[ServerSync]" << "Event delivered:" << event.eventType << "camera:" << event.cameraLabel;
}

void ServerSyncManager::handleEventPostFailed(const EventPayload& event, const QString& error)
{
    if (!eventQueue.isEmpty())
        eventQueue.head().attempts += 1;
    const bool dropEvent = eventQueue.isEmpty() || eventQueue.head().attempts >= maxEventAttempts;
    if (dropEvent && !eventQueue.isEmpty())
        eventQueue.dequeue();
    eventRequestActive = false;
    emit errorMessage(tr("Failed to post event \"%1\": %2").arg(event.eventType, error));
    if (!dropEvent)
        QTimer::singleShot(2000, this, &ServerSyncManager::flushEventQueue);
    qWarning().noquote() << "[ServerSync]" << "Event delivery failed for" << event.eventType << ":" << error
                         << (dropEvent ? " (dropped)" : " (will retry)");
}

void ServerSyncManager::handleSyncTick()
{
    if (!ensureAuthenticated())
        return;
    requestPersonsRefresh();
    requestCamerasRefresh();
    flushEventQueue();
    qInfo() << "[ServerSync]" << "Manual sync executed";
}

void ServerSyncManager::handleFrameProcessed(int cameraId, const QImage&, const QVector<Detection>& detections, const QSize&)
{
    enqueueDetections(cameraId, detections);
    if (!detections.isEmpty())
        qInfo() << "[ServerSync]" << "Queued" << detections.size() << "detections from camera" << cameraId;
}

void ServerSyncManager::handleAlertPosted()
{
    qInfo() << "[ServerSync]" << "Alert posted to server";
    emit alertSubmitted();
}

void ServerSyncManager::handleAlertFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Alert submission failed:" << error;
    emit alertSubmissionFailed(error);
}

void ServerSyncManager::handlePersonCreated(const PersonRecord& person)
{
    qInfo() << "[ServerSync]" << "Person created on server:" << person.name;
    emit personSubmitted(person);
}

void ServerSyncManager::handlePersonFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Person submission failed:" << error;
    emit personSubmissionFailed(error);
}

void ServerSyncManager::handlePersonUpdated(const PersonRecord& person)
{
    qInfo() << "[ServerSync]" << "Person updated on server:" << person.name;
    emit statusMessage(tr("Updated \"%1\"").arg(person.name));
    requestImmediatePersonsRefresh();
}

void ServerSyncManager::handlePersonUpdateFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Person update failed:" << error;
    emit errorMessage(tr("Failed to update person: %1").arg(error));
}

void ServerSyncManager::handlePersonDeleted(const QString& personId)
{
    qInfo() << "[ServerSync]" << "Person deleted:" << personId;
    emit statusMessage(tr("Deleted person from server."));
    requestImmediatePersonsRefresh();
}

void ServerSyncManager::handlePersonDeleteFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Person delete failed:" << error;
    emit errorMessage(tr("Failed to delete person: %1").arg(error));
}

void ServerSyncManager::handleEmbeddingsFetched(const QList<EmbeddingRecord>& embeddings)
{
    embeddingsRequestActive = false;
    embeddingsCache = embeddings;
    QString persistError;
    if (writeEmbeddingsFile(embeddings, &persistError)) {
        if (aiProcessor && !embeddingsPath.isEmpty())
            aiProcessor->loadKnownEmbeddings(embeddingsPath);
        emit statusMessage(tr("Synced %1 embeddings").arg(embeddings.size()));
        qInfo() << "[ServerSync]" << "Embeddings sync completed:" << embeddings.size();
    } else {
        if (persistError.isEmpty())
            persistError = tr("Unknown error");
        emit errorMessage(tr("Failed to persist embeddings data: %1").arg(persistError));
    }
}

void ServerSyncManager::handleEmbeddingsFetchFailed(const QString& error)
{
    embeddingsRequestActive = false;
    qWarning() << "[ServerSync]" << "Embeddings fetch failed:" << error;
    emit errorMessage(tr("Failed to fetch embeddings: %1").arg(error));
}

void ServerSyncManager::handleEmbeddingPosted()
{
    qInfo() << "[ServerSync]" << "Embedding uploaded successfully";
    requestEmbeddingsRefresh();
    emit embeddingUploaded();
}

void ServerSyncManager::handleEmbeddingFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Embedding upload failed:" << error;
    emit errorMessage(tr("Failed to upload embedding: %1").arg(error));
    emit embeddingUploadFailed(error);
}

void ServerSyncManager::handleAvatarUploaded()
{
    qInfo() << "[ServerSync]" << "Avatar uploaded successfully";
    emit avatarUploaded();
}

void ServerSyncManager::handleAvatarUploadFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Avatar upload failed:" << error;
    emit errorMessage(tr("Failed to upload avatar: %1").arg(error));
    emit avatarUploadFailed(error);
}

void ServerSyncManager::handleCamerasFetched(const QList<CameraRecord>& cameras)
{
    camerasRequestActive = false;
    camerasCache = cameras;
    emit camerasUpdated(cameras);
    emit statusMessage(tr("Synced %1 camera records").arg(cameras.size()));
    qInfo() << "[ServerSync]" << "Cameras sync completed:" << cameras.size();
}

void ServerSyncManager::handleCamerasFetchFailed(const QString& error)
{
    camerasRequestActive = false;
    qWarning() << "[ServerSync]" << "Cameras fetch failed:" << error;
    emit errorMessage(tr("Failed to fetch cameras: %1").arg(error));
}

void ServerSyncManager::handleCameraCreated(const CameraRecord& camera)
{
    qInfo() << "[ServerSync]" << "Camera created on server:" << camera.name << camera.streamUrl;
    emit cameraSubmitted(camera);
    requestCamerasRefresh();
}

void ServerSyncManager::handleCameraCreateFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Camera submission failed:" << error;
    emit cameraSubmissionFailed(error);
}

void ServerSyncManager::handleCameraDeleted(const QString& cameraId)
{
    qInfo() << "[ServerSync]" << "Camera deleted on server:" << cameraId;
    emit statusMessage(tr("Camera removed from server."));
    emit cameraDeleted(cameraId);
    requestImmediateCamerasRefresh();
}

void ServerSyncManager::handleCameraDeleteFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Camera delete failed:" << error;
    emit errorMessage(tr("Failed to delete camera: %1").arg(error));
    emit cameraDeleteFailed(error);
}

void ServerSyncManager::handleCameraUpdated(const CameraRecord& camera)
{
    qInfo() << "[ServerSync]" << "Camera updated on server:" << camera.id << camera.status;
    emit cameraUpdated(camera);
    requestImmediateCamerasRefresh();
}

void ServerSyncManager::handleCameraUpdateFailed(const QString& error)
{
    qWarning() << "[ServerSync]" << "Camera update failed:" << error;
    emit errorMessage(tr("Failed to update camera: %1").arg(error));
    emit cameraUpdateFailed(error);
}

QString ServerSyncManager::cameraSourceForLocal(int cameraId) const
{
    if (!cameraManager)
        return {};
    return cameraManager->cameraSource(cameraId);
}

QString ServerSyncManager::cameraIdForStream(const QString& streamUrl) const
{
    const QString normalized = streamUrl.trimmed().toLower();
    if (normalized.isEmpty())
        return {};
    for (const auto& record : camerasCache) {
        if (record.streamUrl.trimmed().toLower() == normalized)
            return record.id;
    }
    return {};
}

QString ServerSyncManager::cameraIdForLocal(int cameraId) const
{
    const QString source = cameraSourceForLocal(cameraId);
    return cameraIdForStream(source);
}

void ServerSyncManager::enqueueDetections(int cameraId, const QVector<Detection>& detections)
{
    if (detections.isEmpty())
        return;

    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const Detection& detection : detections) {
        EventPayload payload;
        const QString category = detection.category.isEmpty() ? QStringLiteral("Detection") : detection.category;
        payload.eventType = QStringLiteral("%1:%2").arg(category, detection.label);
        payload.detectionLabel = detection.label;
        payload.category = detection.category;
        payload.confidence = detection.confidence;
        const QString source = cameraSourceForLocal(cameraId);
        payload.cameraLabel = source.isEmpty() ? (cameraId >= 0 ? QString::number(cameraId) : QString()) : source;
        payload.cameraId = cameraIdForStream(source);
        payload.timestamp = now;
        eventQueue.enqueue({ payload, 0 });
        if (eventQueue.size() > maxEventQueueSize)
            eventQueue.dequeue();
    }

    flushEventQueue();
}

void ServerSyncManager::flushEventQueue()
{
    if (eventRequestActive || eventQueue.isEmpty())
        return;
    if (!client->isAuthenticated())
        return;

    eventRequestActive = true;
    qInfo() << "[ServerSync]" << "Flushing event queue. pending:" << eventQueue.size();
    client->postEvent(eventQueue.head().payload);
}

void ServerSyncManager::requestPersonsRefresh()
{
    if (personsRequestActive)
        return;
    if (!client->isAuthenticated())
        return;
    personsRequestActive = true;
    client->fetchPersons();
}

void ServerSyncManager::requestEmbeddingsRefresh()
{
    if (embeddingsRequestActive)
        return;
    if (!client->isAuthenticated())
        return;
    embeddingsRequestActive = true;
    client->fetchEmbeddings();
}

void ServerSyncManager::requestCamerasRefresh()
{
    if (camerasRequestActive)
        return;
    if (!client->isAuthenticated())
        return;
    camerasRequestActive = true;
    client->fetchCameras();
}

bool ServerSyncManager::ensureAuthenticated()
{
    if (client->isAuthenticated())
        return true;
    if (loginInProgress)
        return false;
    if (email.isEmpty() || password.isEmpty()) {
        emit errorMessage(tr("Server credentials are missing. Update %1").arg(configPath.isEmpty() ? defaultConfigPath() : configPath));
        return false;
    }
    loginInProgress = true;
    qInfo() << "[ServerSync]" << "Requesting authentication for" << email;
    emit statusMessage(tr("Connecting to %1").arg(serverUrl.toString()));
    client->login(email, password);
    return false;
}

bool ServerSyncManager::writeEmbeddingsFile(const QList<EmbeddingRecord>& embeddings, QString* errorOut) const
{
    auto setError = [&](const QString& message) {
        if (errorOut)
            *errorOut = message;
    };
    if (embeddingsPath.isEmpty()) {
        setError(tr("Embeddings path is not configured"));
        return false;
    }
    QJsonArray array;
    for (const auto& record : embeddings) {
        const PersonRecord person = personsById.value(record.personId);
        if (person.id.isEmpty() || record.vector.isEmpty())
            continue;
        QJsonObject obj;
        obj.insert(QStringLiteral("id"), person.id);
        obj.insert(QStringLiteral("name"), person.name.isEmpty() ? person.id : person.name);
        obj.insert(QStringLiteral("samples"), 1);
        if (!person.imageUrl.isEmpty())
            obj.insert(QStringLiteral("image"), person.imageUrl);
        QJsonArray vecArray;
        for (float v : record.vector)
            vecArray.append(v);
        obj.insert(QStringLiteral("embedding"), vecArray);
        array.append(obj);
    }
    QFile file(embeddingsPath);
    QDir dir(QFileInfo(file).dir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        const QString message = tr("Failed to create embeddings directory: %1").arg(dir.absolutePath());
        qWarning() << "[ServerSync]" << message;
        setError(message);
        return false;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString message = tr("Unable to open embeddings file %1: %2").arg(embeddingsPath, file.errorString());
        qWarning() << "[ServerSync]" << message;
        setError(message);
        return false;
    }
    const QByteArray payload = QJsonDocument(array).toJson(QJsonDocument::Indented);
    if (file.write(payload) < 0) {
        const QString message = tr("Failed to write embeddings file %1: %2").arg(embeddingsPath, file.errorString());
        qWarning() << "[ServerSync]" << message;
        setError(message);
        return false;
    }
    return true;
}

PersonRecord ServerSyncManager::personById(const QString& id) const
{
    return personsById.value(id);
}
