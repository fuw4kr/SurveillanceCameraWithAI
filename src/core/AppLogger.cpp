#include "AppLogger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QScopedPointer>
#include <QTextStream>
#include <QThread>

namespace {
QScopedPointer<QFile> g_logFile;
QMutex g_logMutex;
QString g_logPath;

QString levelToString(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO ");
    case QtWarningMsg:
        return QStringLiteral("WARN ");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("LOG  ");
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const Qt::HANDLE threadHandle = QThread::currentThreadId();
#else
    const Qt::HANDLE threadHandle = QThread::currentThreadId();
#endif
    const QString threadId = QStringLiteral("0x%1").arg(reinterpret_cast<quintptr>(threadHandle), 0, 16);
    QString location;
    if (context.file && *context.file)
        location = QStringLiteral("%1:%2").arg(QString::fromUtf8(context.file)).arg(context.line);
    else
        location = QStringLiteral("<unknown>");

    QString formatted = QStringLiteral("%1 [%2] [%3] %4 (%5)")
                            .arg(timestamp,
                                levelToString(type),
                                threadId,
                                message)
                            .arg(location);

    {
        QMutexLocker locker(&g_logMutex);
        if (g_logFile && g_logFile->isOpen()) {
            QTextStream stream(g_logFile.data());
            stream << formatted << Qt::endl;
            g_logFile->flush();
        }
    }

    QTextStream stderrStream(stderr);
    stderrStream << formatted << Qt::endl;

    if (type == QtFatalMsg)
        abort();
}
}

namespace AppLogger {
void initialize(const QString& logDirectory)
{
    QString dirPath = logDirectory;
    if (dirPath.isEmpty())
        dirPath = QCoreApplication::applicationDirPath() + QStringLiteral("/logs");

    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    const QString fileName = QStringLiteral("app_%1.log")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString absolutePath = dir.absoluteFilePath(fileName);

    g_logFile.reset(new QFile(absolutePath));
    if (!g_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream(stderr) << "Failed to open log file: " << absolutePath << Qt::endl;
        g_logFile.reset();
    } else {
        g_logPath = absolutePath;
    }

    qInstallMessageHandler(messageHandler);
    qInfo() << "Application logging initialized at" << g_logPath;
}

QString logFilePath()
{
    QMutexLocker locker(&g_logMutex);
    return g_logPath;
}
}
