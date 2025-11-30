#include <gtest/gtest.h>

#include "core/ServerTypes.h"

TEST(ServerTypesTest, PersonRecordValidity)
{
    PersonRecord invalid;
    EXPECT_FALSE(invalid.isValid());

    PersonRecord valid;
    valid.id = QStringLiteral("123");
    EXPECT_TRUE(valid.isValid());
}

TEST(ServerTypesTest, DefaultValuesAreInitialized)
{
    PersonRecord person;
    EXPECT_FALSE(person.authorized);
    EXPECT_TRUE(person.id.isEmpty());

    EventPayload event;
    EXPECT_FLOAT_EQ(event.confidence, 0.0f);
    EXPECT_TRUE(event.eventType.isEmpty());

    EmbeddingRecord embedding;
    EXPECT_TRUE(embedding.vector.isEmpty());
    EXPECT_TRUE(embedding.modelName.isEmpty());
}
