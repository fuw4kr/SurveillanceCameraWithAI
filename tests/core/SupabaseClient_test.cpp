#include <gtest/gtest.h>

#include <QDateTime>

#include "core/SupabaseClient.h"

TEST(SupabaseClientTest, AuthenticationStateTransitions)
{
    SupabaseClient client;
    EXPECT_FALSE(client.isAuthenticated());

    client.applySession(QStringLiteral("token"), QDateTime::currentDateTimeUtc().addSecs(60));
    EXPECT_TRUE(client.isAuthenticated());

    client.applySession(QStringLiteral("token"), QDateTime::currentDateTimeUtc().addSecs(-30));
    EXPECT_FALSE(client.isAuthenticated());

    // Expiry not set should be treated as valid until cleared.
    client.applySession(QStringLiteral("token"), QDateTime());
    EXPECT_TRUE(client.isAuthenticated());

    client.clearSession();
    EXPECT_FALSE(client.isAuthenticated());
}
