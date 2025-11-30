#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QEventLoop>
#include <QImage>
#include <QMetaObject>
#include <QSize>
#include <QTimer>

#include "core/AIProcessor.h"
#include "core/DetectionEventController.h"
#include "core/ServerSyncManager.h"

namespace {
void waitMs(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

struct RecordedEvent {
    QString personId;
    int cameraId = -1;
    bool active = false;
    QDateTime timestamp;
};

class FakeServerSyncManager : public ServerSyncManager {
public:
    explicit FakeServerSyncManager(const QHash<QString, QString>& nameMap, QObject* parent = nullptr)
        : ServerSyncManager(parent)
    {
        for (auto it = nameMap.constBegin(); it != nameMap.constEnd(); ++it)
            mapping.insert(it.key().toLower(), it.value());
    }

    QString personIdForName(const QString& name) const override
    {
        return mapping.value(name.toLower());
    }

    void sendDetectionStatus(const QString& personId, int cameraId, bool active, const QDateTime& timestamp) override
    {
        events.push_back(RecordedEvent{ personId, cameraId, active, timestamp });
    }

    QVector<RecordedEvent> events;

private:
    QHash<QString, QString> mapping;
};

Detection makeFace(const QString& label, const QString& category = QStringLiteral("Face"))
{
    Detection d;
    d.label = label;
    d.category = category;
    d.confidence = 0.9f;
    return d;
}

void invokeHandleFrame(DetectionEventController& controller, int cameraId, const QVector<Detection>& detections)
{
    QMetaObject::invokeMethod(&controller, "handleFrame", Qt::DirectConnection,
        Q_ARG(int, cameraId),
        Q_ARG(QImage, QImage()),
        Q_ARG(QVector<Detection>, detections),
        Q_ARG(QSize, QSize()));
}
}

TEST(DetectionEventControllerTest, EmitsActiveForKnownFace)
{
    FakeServerSyncManager sync({ { "alice", "person-1" } });
    DetectionEventController controller(nullptr, &sync, nullptr, 200);

    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral("Alice")) });

    ASSERT_EQ(sync.events.size(), 1);
    EXPECT_TRUE(sync.events[0].active);
    EXPECT_EQ(sync.events[0].personId, QStringLiteral("person-1"));
    EXPECT_EQ(sync.events[0].cameraId, 1);
}

TEST(DetectionEventControllerTest, IgnoresNonFaceOrUnknownLabels)
{
    FakeServerSyncManager sync({ { "alice", "person-1" } });
    DetectionEventController controller(nullptr, &sync, nullptr, 200);

    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral(""), QStringLiteral("Face")) });
    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral("Face"), QStringLiteral("Face")) });
    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral("Alice"), QStringLiteral("Object")) });
    invokeHandleFrame(controller, 1, { makeFace(QStringLiteral("Unknown"), QStringLiteral("Face")) });

    EXPECT_TRUE(sync.events.isEmpty());
}

TEST(DetectionEventControllerTest, DoesNotDuplicateActiveEvents)
{
    FakeServerSyncManager sync({ { "alice", "person-1" } });
    DetectionEventController controller(nullptr, &sync, nullptr, 500);

    invokeHandleFrame(controller, 2, { makeFace(QStringLiteral("Alice")) });
    invokeHandleFrame(controller, 2, { makeFace(QStringLiteral("Alice")) });

    ASSERT_EQ(sync.events.size(), 1);
    EXPECT_TRUE(sync.events[0].active);
}

TEST(DetectionEventControllerTest, ExpiresActiveEventsAfterTimeout)
{
    FakeServerSyncManager sync({ { "alice", "person-1" } });
    DetectionEventController controller(nullptr, &sync, nullptr, 80);

    invokeHandleFrame(controller, 3, { makeFace(QStringLiteral("Alice")) });
    ASSERT_EQ(sync.events.size(), 1);
    EXPECT_TRUE(sync.events[0].active);

    waitMs(120); // exceed timeout
    invokeHandleFrame(controller, 3, {});

    ASSERT_EQ(sync.events.size(), 2);
    EXPECT_FALSE(sync.events[1].active);
    EXPECT_EQ(sync.events[1].personId, QStringLiteral("person-1"));
}

TEST(DetectionEventControllerTest, RefreshesLastSeenAndPreventsPrematureExpiry)
{
    FakeServerSyncManager sync({ { "alice", "person-1" } });
    DetectionEventController controller(nullptr, &sync, nullptr, 120);

    invokeHandleFrame(controller, 4, { makeFace(QStringLiteral("Alice")) });
    waitMs(60); // below timeout
    invokeHandleFrame(controller, 4, { makeFace(QStringLiteral("Alice")) });

    ASSERT_EQ(sync.events.size(), 1);
    waitMs(150); // now exceed timeout from last seen
    invokeHandleFrame(controller, 4, {});

    ASSERT_EQ(sync.events.size(), 2);
    EXPECT_FALSE(sync.events[1].active);
}
