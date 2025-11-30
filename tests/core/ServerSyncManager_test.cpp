#include <gtest/gtest.h>

#include <QMetaObject>
#include <QVector>
#include <QColor>

#include "core/ServerSyncManager.h"
#include "core/AIProcessor.h"

TEST(ServerSyncManagerTest, PersonLookupPopulatedFromFetch)
{
    ServerSyncManager sync;

    QList<PersonRecord> persons;
    PersonRecord alice;
    alice.id = QStringLiteral("id-alice");
    alice.name = QStringLiteral("Alice");
    persons << alice;

    QMetaObject::invokeMethod(&sync, "handlePersonsFetched", Qt::DirectConnection,
        Q_ARG(QList<PersonRecord>, persons));

    EXPECT_EQ(sync.personIdForName(QStringLiteral("alice")), QStringLiteral("id-alice"));
    EXPECT_EQ(sync.personIdForName(QStringLiteral("ALICE")), QStringLiteral("id-alice"));
}

TEST(ServerSyncManagerTest, PersonLookupClearsWhenEmpty)
{
    ServerSyncManager sync;

    QList<PersonRecord> persons;
    PersonRecord bob;
    bob.id = QStringLiteral("id-bob");
    bob.name = QStringLiteral("Bob");
    persons << bob;

    QMetaObject::invokeMethod(&sync, "handlePersonsFetched", Qt::DirectConnection,
        Q_ARG(QList<PersonRecord>, persons));
    EXPECT_EQ(sync.personIdForName(QStringLiteral("Bob")), QStringLiteral("id-bob"));

    persons.clear();
    QMetaObject::invokeMethod(&sync, "handlePersonsFetched", Qt::DirectConnection,
        Q_ARG(QList<PersonRecord>, persons));
    EXPECT_TRUE(sync.personIdForName(QStringLiteral("Bob")).isEmpty());
}

class ServerSyncManagerTestAccessor {
public:
    static void enqueue(ServerSyncManager& s, int cameraId, const QVector<Detection>& detections)
    {
        s.enqueueDetections(cameraId, detections);
    }

    static int queueSize(const ServerSyncManager& s)
    {
        return s.eventQueue.size();
    }

    static EventPayload frontPayload(const ServerSyncManager& s)
    {
        return s.eventQueue.isEmpty() ? EventPayload{} : s.eventQueue.head().payload;
    }

    static int maxQueueSize(const ServerSyncManager& s)
    {
        return s.maxEventQueueSize;
    }
};

static Detection makeDetection(const QString& label, const QString& category = QStringLiteral("Face"), float confidence = 0.9f)
{
    Detection d;
    d.label = label;
    d.category = category;
    d.confidence = confidence;
    d.color = QColor(255, 0, 0);
    return d;
}

TEST(ServerSyncManagerTest, EnqueueDetectionsAddsEvents)
{
    ServerSyncManager sync;
    QVector<Detection> detections;
    detections.append(makeDetection(QStringLiteral("Bob")));
    detections.append(makeDetection(QStringLiteral("Alice"), QStringLiteral("Person"), 0.7f));

    ServerSyncManagerTestAccessor::enqueue(sync, 1, detections);

    ASSERT_EQ(ServerSyncManagerTestAccessor::queueSize(sync), 2);
    const EventPayload payload = ServerSyncManagerTestAccessor::frontPayload(sync);
    EXPECT_EQ(payload.eventType, QStringLiteral("Face:Bob"));
    EXPECT_EQ(payload.detectionLabel, QStringLiteral("Bob"));
    EXPECT_EQ(payload.category, QStringLiteral("Face"));
    EXPECT_EQ(payload.cameraLabel, QStringLiteral("1"));
}

TEST(ServerSyncManagerTest, EnqueueDetectionsTrimsQueue)
{
    ServerSyncManager sync;
    const int maxSize = ServerSyncManagerTestAccessor::maxQueueSize(sync);
    QVector<Detection> detections;
    detections.append(makeDetection(QStringLiteral("Label")));

    for (int i = 0; i < maxSize + 5; ++i)
        ServerSyncManagerTestAccessor::enqueue(sync, 0, detections);

    EXPECT_EQ(ServerSyncManagerTestAccessor::queueSize(sync), maxSize);
}
