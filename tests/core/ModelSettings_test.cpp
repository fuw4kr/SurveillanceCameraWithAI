#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/modelSettings.h"

namespace {
QString normalize(const QString& path)
{
    QString result = path;
    result.replace('\\', '/');
    return result;
}
}

class ModelSettingsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        const QString appDir = QCoreApplication::applicationDirPath();
        configDirPath = QDir(appDir).absoluteFilePath(QStringLiteral("config"));
        settingsFilePath = QDir(configDirPath).absoluteFilePath(QStringLiteral("model_settings.json"));
        QFile::remove(settingsFilePath);
    }

    void TearDown() override
    {
        QFile::remove(settingsFilePath);
    }

    QString configDirPath;
    QString settingsFilePath;
};

TEST_F(ModelSettingsTest, LoadReturnsEmptyWhenFileMissing)
{
    ASSERT_FALSE(QFile::exists(settingsFilePath));
    const ModelSettings settings = ModelSettings::load();
    EXPECT_TRUE(settings.detectionModel.isEmpty());
    EXPECT_TRUE(settings.detectionConfig.isEmpty());
    EXPECT_TRUE(settings.embeddingModel.isEmpty());
}

TEST_F(ModelSettingsTest, LoadReturnsEmptyOnInvalidJson)
{
    QDir().mkpath(QFileInfo(settingsFilePath).absoluteDir().absolutePath());
    QFile file(settingsFilePath);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Text));
    file.write("not a json");
    file.close();

    const ModelSettings settings = ModelSettings::load();
    EXPECT_TRUE(settings.detectionModel.isEmpty());
    EXPECT_TRUE(settings.detectionConfig.isEmpty());
    EXPECT_TRUE(settings.embeddingModel.isEmpty());
}

TEST_F(ModelSettingsTest, SaveAndLoadRoundTripNormalizesPaths)
{
    ModelSettings settings;
    settings.detectionModel = QStringLiteral("models\\det.onnx");
    settings.detectionConfig = QStringLiteral("configs\\det.json");
    settings.embeddingModel = QStringLiteral("models\\embed.onnx");

    ASSERT_TRUE(settings.save());
    ASSERT_TRUE(QFile::exists(settingsFilePath));

    QFile file(settingsFilePath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const auto json = QJsonDocument::fromJson(file.readAll()).object();
    EXPECT_EQ(QStringLiteral("models/det.onnx"), json.value(QStringLiteral("detectionModel")).toString());
    EXPECT_EQ(QStringLiteral("configs/det.json"), json.value(QStringLiteral("detectionConfig")).toString());
    EXPECT_EQ(QStringLiteral("models/embed.onnx"), json.value(QStringLiteral("embeddingModel")).toString());

    const ModelSettings loaded = ModelSettings::load();
    EXPECT_EQ(QStringLiteral("models/det.onnx"), loaded.detectionModel);
    EXPECT_EQ(QStringLiteral("configs/det.json"), loaded.detectionConfig);
    EXPECT_EQ(QStringLiteral("models/embed.onnx"), loaded.embeddingModel);
}

TEST_F(ModelSettingsTest, ResolveRelativePathUsesApplicationDir)
{
    const QString relative = QStringLiteral("models/test.onnx");
    const QString expected = normalize(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(relative));
    EXPECT_EQ(expected, ModelSettings::resolvePath(relative));
}

TEST_F(ModelSettingsTest, ResolveEmptyReturnsEmpty)
{
    EXPECT_TRUE(ModelSettings::resolvePath(QString()).isEmpty());
}

TEST_F(ModelSettingsTest, ResolveAbsolutePathPreservesAbsolute)
{
    const QString absolute = normalize(QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("abs/test.onnx")));
    EXPECT_EQ(absolute, ModelSettings::resolvePath(absolute));
}

TEST_F(ModelSettingsTest, ToRelativeStripsApplicationDirPrefix)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString absolute = QDir(appDir).absoluteFilePath(QStringLiteral("models/test.onnx"));
    EXPECT_EQ(QStringLiteral("models/test.onnx"), ModelSettings::toRelative(absolute));
}

TEST_F(ModelSettingsTest, ToRelativeReturnsEmptyForEmptyInput)
{
    EXPECT_TRUE(ModelSettings::toRelative(QString()).isEmpty());
}

TEST_F(ModelSettingsTest, ToRelativeNormalizesAlreadyRelative)
{
    const QString relative = QStringLiteral("..\\shared\\model.onnx");
    EXPECT_EQ(QStringLiteral("../shared/model.onnx"), ModelSettings::toRelative(relative));
}
