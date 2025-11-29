#ifndef MODELSETTINGS_H
#define MODELSETTINGS_H

#include <QString>

struct ModelSettings {
    QString detectionModel;
    QString detectionConfig;
    QString embeddingModel;

    static ModelSettings load();
    bool save() const;

    static QString resolvePath(const QString& path);
    static QString toRelative(const QString& absolutePath);

private:
    static QString settingsFilePath();
};

#endif // MODELSETTINGS_H
