#include "modelSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString normalizedPath(const QString& path)
{
    QString normalized = path;
    normalized.replace('\\', '/');
    return normalized;
}
}

ModelSettings ModelSettings::load()
{
    ModelSettings settings;
    QFile file(settingsFilePath());
    if (!file.exists())
        return settings;
    if (!file.open(QIODevice::ReadOnly))
        return settings;

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return settings;

    const QJsonObject obj = doc.object();
    settings.detectionModel = obj.value(QStringLiteral("detectionModel")).toString();
    settings.detectionConfig = obj.value(QStringLiteral("detectionConfig")).toString();
    settings.embeddingModel = obj.value(QStringLiteral("embeddingModel")).toString();
    settings.genderModel = obj.value(QStringLiteral("genderModel")).toString();
    return settings;
}

bool ModelSettings::save() const
{
    QFileInfo info(settingsFilePath());
    QDir dir(info.absoluteDir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;

    QJsonObject obj;
    obj.insert(QStringLiteral("detectionModel"), normalizedPath(detectionModel));
    obj.insert(QStringLiteral("detectionConfig"), normalizedPath(detectionConfig));
    obj.insert(QStringLiteral("embeddingModel"), normalizedPath(embeddingModel));
    obj.insert(QStringLiteral("genderModel"), normalizedPath(genderModel));

    QFile file(info.absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

QString ModelSettings::resolvePath(const QString& path)
{
    if (path.isEmpty())
        return {};
    QFileInfo info(path);
    if (info.isAbsolute())
        return normalizedPath(info.absoluteFilePath());
    const QDir base(QCoreApplication::applicationDirPath());
    return normalizedPath(base.absoluteFilePath(path));
}

QString ModelSettings::toRelative(const QString& absolutePath)
{
    if (absolutePath.isEmpty())
        return {};
    QFileInfo info(absolutePath);
    if (!info.isAbsolute())
        return normalizedPath(absolutePath);
    const QDir base(QCoreApplication::applicationDirPath());
    return normalizedPath(base.relativeFilePath(info.absoluteFilePath()));
}

QString ModelSettings::settingsFilePath()
{
    const QString dir = QCoreApplication::applicationDirPath() + QStringLiteral("/config");
    return dir + QStringLiteral("/model_settings.json");
}
