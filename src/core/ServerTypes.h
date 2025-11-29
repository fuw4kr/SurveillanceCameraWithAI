#ifndef SERVERTYPES_H
#define SERVERTYPES_H

#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariant>

struct PersonRecord {
    QString id;
    QDateTime registeredAt;
    QString name;
    QString role;
    QString imageUrl;
    bool authorized = false;
    QDateTime lastSeen;

    bool isValid() const { return !id.isEmpty(); }
};

struct EventPayload {
    QString eventType;
    QString detectionLabel;
    QString category;
    float confidence = 0.0f;
    QString cameraLabel;
    QDateTime timestamp;
};

Q_DECLARE_METATYPE(PersonRecord)
Q_DECLARE_METATYPE(QList<PersonRecord>)
Q_DECLARE_METATYPE(EventPayload)

#endif // SERVERTYPES_H
