#ifndef SERVERSYNCMANAGER_H
#define SERVERSYNCMANAGER_H

#include "AIProcessor.h"
#include "ServerTypes.h"
#include "SupabaseClient.h"
#include <QDateTime>
#include <QImage>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QSize>
#include <QVector>

class ServerSyncManager : public QObject
{
    Q_OBJECT

public:
    explicit ServerSyncManager(QObject* parent = nullptr);

    bool loadConfig(const QString& path = QString());
    void setAiProcessor(AIProcessor* processor);
    void start();
    void requestImmediatePersonsRefresh();
    void setCredentials(const QString& email, const QString& password);
    void applySessionToken(const QString& token, const QDateTime& expiresAt);
    void clearSession();

signals:
    void personsUpdated(const QList<PersonRecord>& persons);
    void statusMessage(const QString& status);
    void errorMessage(const QString& error);

private slots:
    void handleLoginResult(const AuthResult& result);
    void handlePersonsFetched(const QList<PersonRecord>& persons);
    void handlePersonsFetchFailed(const QString& error);
    void handleEventPosted(const EventPayload& event);
    void handleEventPostFailed(const EventPayload& event, const QString& error);
    void handleSyncTick();
    void handleFrameProcessed(int cameraId, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);

private:
    struct QueuedEvent {
        EventPayload payload;
        int attempts = 0;
    };

    void enqueueDetections(int cameraId, const QVector<Detection>& detections);
    void flushEventQueue();
    void requestPersonsRefresh();
    bool ensureAuthenticated();

    SupabaseClient* client = nullptr;
    AIProcessor* aiProcessor = nullptr;
    QTimer syncTimer;
    QQueue<QueuedEvent> eventQueue;
    QList<PersonRecord> personsCache;

    QString email;
    QString password;
    QUrl serverUrl{ QStringLiteral("https://myserver-tc2d.onrender.com") };
    QString configPath;
    int syncIntervalMs = 5000;
    bool configLoaded = false;
    bool loginInProgress = false;
    bool personsRequestActive = false;
    bool eventRequestActive = false;
    bool manualCredentialsProvided = false;
    const int maxEventQueueSize = 200;
    const int maxEventAttempts = 3;
};

#endif // SERVERSYNCMANAGER_H
