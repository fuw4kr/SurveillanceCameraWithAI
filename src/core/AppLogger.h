#ifndef APPLOGGER_H
#define APPLOGGER_H

#include <QString>

namespace AppLogger {
void initialize(const QString& logDirectory = QString());
QString logFilePath();
}

#endif // APPLOGGER_H
