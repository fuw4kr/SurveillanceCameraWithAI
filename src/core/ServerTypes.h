#ifndef SERVERTYPES_H
#define SERVERTYPES_H

#include <QDateTime>
#include <QList>
#include <QVector>
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
    QString cameraId;
    QString snapshotUrl;
    QDateTime timestamp;
    QString personId;
};

struct EmbeddingRecord {
    QString id;
    QString personId;
    QString modelName;
    QVector<float> vector;
    QDateTime createdAt;
};

struct CameraRecord {
    QString id;
    QString name;
    QString ipAddress;
    QString location;
    QString status;
    QString streamUrl;
    QDateTime createdAt;
};

Q_DECLARE_METATYPE(PersonRecord)
Q_DECLARE_METATYPE(QList<PersonRecord>)
Q_DECLARE_METATYPE(EventPayload)
Q_DECLARE_METATYPE(EmbeddingRecord)
Q_DECLARE_METATYPE(QList<EmbeddingRecord>)
Q_DECLARE_METATYPE(CameraRecord)
Q_DECLARE_METATYPE(QList<CameraRecord>)

#endif // SERVERTYPES_H
