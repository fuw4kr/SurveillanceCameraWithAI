#include <gtest/gtest.h>

#include <QRect>
#include <QString>
#include <QVector>

#include "ui/FaceAlertController.h"
#include "core/ServerTypes.h"

class FaceAlertControllerTestAccessor {
public:
    static bool isRecentlyPrompted(FaceAlertController& c, int cameraId, const QRect& rect, qint64 now)
    {
        return c.isRecentlyPrompted(cameraId, rect, now);
    }

    static void addRecent(FaceAlertController& c, int cameraId, const QPoint& center, qint64 ts)
    {
        c.recentFaces.push_back({ cameraId, center, ts });
    }

    static void pruneRecent(FaceAlertController& c, qint64 now)
    {
        c.pruneRecent(now);
    }

    static QString nextUnknown(FaceAlertController& c)
    {
        return c.nextUnknownLabel();
    }

    static void handlePersons(FaceAlertController& c, const QList<PersonRecord>& persons)
    {
        c.handlePersonsDirectoryUpdated(persons);
    }

    static void setCounter(FaceAlertController& c, int value)
    {
        c.unknownCounter = value;
    }

    static int queueSize(FaceAlertController& c)
    {
        return c.pendingQueue.size();
    }

    static void enqueue(FaceAlertController& c, const PendingFaceAlert& alert)
    {
        c.enqueueFace(alert);
    }
};

TEST(FaceAlertControllerTest, IsRecentlyPromptedMatchesNearbyWithinWindow)
{
    FaceAlertController controller(nullptr, nullptr, nullptr);
    const qint64 now = 10'000;
    FaceAlertControllerTestAccessor::addRecent(controller, 1, QPoint(100, 100), now - 100);

    EXPECT_TRUE(FaceAlertControllerTestAccessor::isRecentlyPrompted(controller, 1, QRect(80, 80, 40, 40), now));
    EXPECT_FALSE(FaceAlertControllerTestAccessor::isRecentlyPrompted(controller, 1, QRect(300, 300, 20, 20), now));
}

TEST(FaceAlertControllerTest, PruneRecentRemovesOldEntries)
{
    FaceAlertController controller(nullptr, nullptr, nullptr);
    const qint64 now = 10'000;
    FaceAlertControllerTestAccessor::addRecent(controller, 1, QPoint(0, 0), now - 7000);
    FaceAlertControllerTestAccessor::addRecent(controller, 1, QPoint(0, 0), now - 1000);

    FaceAlertControllerTestAccessor::pruneRecent(controller, now);

    EXPECT_EQ(controller.recentFaces.size(), 1);
    EXPECT_GE(controller.recentFaces.front().timestampMs, now - 6000);
}

TEST(FaceAlertControllerTest, NextUnknownLabelRespectsExistingUnknowns)
{
    FaceAlertController controller(nullptr, nullptr, nullptr);
    QList<PersonRecord> persons;
    PersonRecord p1; p1.name = QStringLiteral("UNKNOWN_3");
    PersonRecord p2; p2.name = QStringLiteral("Alice");
    persons << p1 << p2;

    FaceAlertControllerTestAccessor::handlePersons(controller, persons);
    const QString label = FaceAlertControllerTestAccessor::nextUnknown(controller);
    EXPECT_EQ(label, QStringLiteral("UNKNOWN_4"));
}

TEST(FaceAlertControllerTest, NextUnknownLabelIncrementsCounter)
{
    FaceAlertController controller(nullptr, nullptr, nullptr);
    FaceAlertControllerTestAccessor::setCounter(controller, 10);

    const QString first = FaceAlertControllerTestAccessor::nextUnknown(controller);
    const QString second = FaceAlertControllerTestAccessor::nextUnknown(controller);

    EXPECT_EQ(first, QStringLiteral("UNKNOWN_10"));
    EXPECT_EQ(second, QStringLiteral("UNKNOWN_11"));
}

class FakeServerSyncForUnknowns : public ServerSyncManager {
public:
    explicit FakeServerSyncForUnknowns(const QSet<QString>& existing, QObject* parent = nullptr)
        : ServerSyncManager(parent)
        , existingNames(existing) {}

    QString personIdForName(const QString& name) const override
    {
        return existingNames.contains(name.toLower()) ? QStringLiteral("conflict") : QString();
    }

private:
    QSet<QString> existingNames;
};

TEST(FaceAlertControllerTest, NextUnknownLabelSkipsConflictsFromServer)
{
    QSet<QString> existing;
    existing.insert(QStringLiteral("unknown_1"));
    existing.insert(QStringLiteral("unknown_2"));

    FakeServerSyncForUnknowns sync(existing);
    FaceAlertController controller(nullptr, &sync, nullptr);
    FaceAlertControllerTestAccessor::setCounter(controller, 1);

    const QString label = FaceAlertControllerTestAccessor::nextUnknown(controller);
    EXPECT_EQ(label, QStringLiteral("UNKNOWN_3"));
}

TEST(FaceAlertControllerTest, EnqueueFaceAddsToQueue)
{
    FaceAlertController controller(nullptr, nullptr, nullptr);
    PendingFaceAlert alert;
    alert.cameraId = 5;
    alert.personLabel = QStringLiteral("Test");
    FaceAlertControllerTestAccessor::enqueue(controller, alert);

    EXPECT_EQ(FaceAlertControllerTestAccessor::queueSize(controller), 1);
}
