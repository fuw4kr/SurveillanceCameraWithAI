#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include "core/AppLogger.h"

namespace {
QString normalize(const QString& path)
{
    QString result = path;
    result.replace('\\', '/');
    return result;
}
}

TEST(AppLoggerTest, InitializesLoggerAndWritesMessages)
{
    const QString uniqueLogDir = QDir(QDir::tempPath())
                                     .absoluteFilePath(QStringLiteral("app_logger_test_%1")
                                                           .arg(QDateTime::currentMSecsSinceEpoch()));

    AppLogger::initialize(uniqueLogDir);
    const QString logPath = AppLogger::logFilePath();

    ASSERT_FALSE(logPath.isEmpty());
    EXPECT_EQ(0, normalize(logPath).indexOf(normalize(uniqueLogDir)));

    qInfo() << "AppLoggerTestMessage";

    QFile file(logPath);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString contents = QString::fromUtf8(file.readAll());
    EXPECT_NE(contents.indexOf(QStringLiteral("AppLoggerTestMessage")), -1);
}

TEST(AppLoggerTest, ReinitializeCreatesNewLogFile)
{
    const QString dir1 = QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("app_logger_test_reinit1"));
    const QString dir2 = QDir(QDir::tempPath()).absoluteFilePath(QStringLiteral("app_logger_test_reinit2"));

    AppLogger::initialize(dir1);
    const QString path1 = AppLogger::logFilePath();
    ASSERT_FALSE(path1.isEmpty());
    EXPECT_TRUE(QFile::exists(path1));

    AppLogger::initialize(dir2);
    const QString path2 = AppLogger::logFilePath();
    ASSERT_FALSE(path2.isEmpty());
    EXPECT_TRUE(QFile::exists(path2));
    EXPECT_NE(normalize(path1), normalize(path2));
}
