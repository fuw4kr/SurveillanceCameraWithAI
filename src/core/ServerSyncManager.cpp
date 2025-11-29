#include "ServerSyncManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
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

    syncTimer.setSingleShot(false);
    syncTimer.setInterval(syncIntervalMs);
    connect(&syncTimer, &QTimer::timeout, this, &ServerSyncManager::handleSyncTick);
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
    if (aiProcessor) {
        connect(aiProcessor, &AIProcessor::frameProcessed,
            this, &ServerSyncManager::handleFrameProcessed,
            Qt::QueuedConnection);
    }
}

void ServerSyncManager::start()
{
    if (!configLoaded)
        loadConfig(QString());
    if (!syncTimer.isActive())
        syncTimer.start();
    handleSyncTick();
    qInfo() << "[ServerSync]" << "Synchronization timer started";
}

void ServerSyncManager::requestImmediatePersonsRefresh()
{
    personsRequestActive = false;
    requestPersonsRefresh();
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
    flushEventQueue();
}

void ServerSyncManager::handlePersonsFetched(const QList<PersonRecord>& persons)
{
    personsCache = persons;
    personsRequestActive = false;
    emit personsUpdated(persons);
    emit statusMessage(tr("Synced %1 person records").arg(persons.size()));
    qInfo() << "[ServerSync]" << "Persons sync completed:" << persons.size() << "records";
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
    flushEventQueue();
    qInfo() << "[ServerSync]" << "Periodic sync executed";
}

void ServerSyncManager::handleFrameProcessed(int cameraId, const QImage&, const QVector<Detection>& detections, const QSize&)
{
    enqueueDetections(cameraId, detections);
    if (!detections.isEmpty())
        qInfo() << "[ServerSync]" << "Queued" << detections.size() << "detections from camera" << cameraId;
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
        payload.cameraLabel = cameraId >= 0 ? QString::number(cameraId) : QString();
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
