#ifndef APPLOGGER_H
#define APPLOGGER_H

/**
 * @file AppLogger.h
 * @brief Central logging bootstrapper for the application.
 *
 * Provides a thin namespace API to initialize Qt logging to a file and query
 * the active log path. This is intended to be invoked during application
 * startup before other subsystems emit logs.
 *
 * @example
 * AppLogger::initialize();
 * qInfo() << "Logger ready at" << AppLogger::logFilePath();
 */

#include <QString>

namespace AppLogger {
/**
 * @brief Configures Qt logging to write to a rotating file.
 *
 * Creates the log directory if needed and installs a message handler so subsequent
 * qDebug/qInfo/qWarning/qCritical calls are persisted.
 *
 * @param logDirectory Optional directory override; defaults to platform temp/app data.
 * @return void
 * @throws None
 * @example AppLogger::initialize("logs");
 */
void initialize(const QString& logDirectory = QString());

/**
 * @brief Returns the absolute path to the currently active log file.
 * @return QString Full file path, or empty if not initialized.
 * @throws None
 * @example QString path = AppLogger::logFilePath();
 */
QString logFilePath();
}

#endif // APPLOGGER_H
