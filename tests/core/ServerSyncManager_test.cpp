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

