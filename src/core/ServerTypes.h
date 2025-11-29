#ifndef SERVERTYPES_H
#define SERVERTYPES_H

/**
 * @file ServerTypes.h
 * @brief Data transfer objects for server synchronization.
 *
 * Defines records for persons, events, and embeddings exchanged with the backend
 * along with Qt metatype declarations for signal/slot transport.
 *
 * @example
 * PersonRecord person{ .id = "123", .name = "Alice", .authorized = true };
 * QVariant v = QVariant::fromValue(person);
 */

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

    /**
     * @brief Indicates whether the record contains a valid id.
     * @return bool True when id is non-empty.
     * @throws None
     */
    bool isValid() const { return !id.isEmpty(); }
};

struct EventPayload {
    QString eventType;
    QString detectionLabel;
    QString category;
    float confidence = 0.0f;
    QString cameraLabel;
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

Q_DECLARE_METATYPE(PersonRecord)
Q_DECLARE_METATYPE(QList<PersonRecord>)
Q_DECLARE_METATYPE(EventPayload)
Q_DECLARE_METATYPE(EmbeddingRecord)
Q_DECLARE_METATYPE(QList<EmbeddingRecord>)

#endif // SERVERTYPES_H
