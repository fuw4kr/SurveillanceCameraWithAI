#ifndef MODELSETTINGS_H
#define MODELSETTINGS_H

/**
 * @file modelSettings.h
 * @brief Persistence helper for detection and embedding model paths.
 *
 * Stores user-selected model/config paths, provides path resolution relative to
 * the application directory, and serializes settings to disk.
 *
 * @example
 * ModelSettings s = ModelSettings::load();
 * s.detectionModel = "assets/models/det.onnx";
 * s.save();
 */

#include <QString>

struct ModelSettings {
    QString detectionModel;
    QString detectionConfig;
    QString embeddingModel;

    /**
     * @brief Loads persisted model settings from disk.
     * @return ModelSettings Populated settings (default constructed on failure).
     * @throws None
     * @example auto s = ModelSettings::load();
     */
    static ModelSettings load();
    /**
     * @brief Saves current model settings to disk.
     * @return bool True if write succeeds.
     * @throws None
     * @example settings.save();
     */
    bool save() const;

    /**
     * @brief Resolves a stored relative path to an absolute path if possible.
     * @param path Relative or absolute path.
     * @return QString Absolute path when resolvable, otherwise the input.
     * @throws None
     * @example QString abs = ModelSettings::resolvePath(modelPath);
     */
    static QString resolvePath(const QString& path);
    /**
     * @brief Converts an absolute path into a relative, portable path.
     * @param absolutePath Absolute file path.
     * @return QString Relative path when under the app dir; otherwise the input.
     * @throws None
     * @example QString rel = ModelSettings::toRelative(absPath);
     */
    static QString toRelative(const QString& absolutePath);

private:
    static QString settingsFilePath();
};

#endif // MODELSETTINGS_H
