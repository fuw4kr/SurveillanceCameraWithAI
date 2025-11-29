#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

/**
 * @file settingsPage.h
 * @brief UI page for selecting detection and embedding models.
 *
 * Provides comboboxes populated from a models directory and emits signals when
 * the user chooses new models/configs.
 *
 * @example
 * SettingsPage* page = new SettingsPage(modelsDir, this);
 * connect(page, &SettingsPage::detectionModelSelected, ...);
 */

#include <QWidget>
#include <QVector>

class QComboBox;
class QPushButton;
class QLabel;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the settings page and loads available model files.
     * @param modelsDirectory Root directory to scan for models.
     * @param parent Optional parent widget.
     */
    explicit SettingsPage(const QString& modelsDirectory, QWidget* parent = nullptr);

    /**
     * @brief Updates UI to reflect currently loaded models.
     * @param detectionModel Active detection model path.
     * @param detectionConfig Associated config path (if any).
     * @param embeddingModel Active embedding model path.
     */
    void setCurrentModels(const QString& detectionModel, const QString& detectionConfig, const QString& embeddingModel);

signals:
    void detectionModelSelected(const QString& modelPath, const QString& configPath);
    void embeddingModelSelected(const QString& modelPath);

private slots:
    void onDetectionChanged(int index);
    void onEmbeddingChanged(int index);
    void onBrowseDetection();
    void onBrowseEmbedding();

private:
    struct DetectionEntry {
        QString label;
        QString modelPath;
        QString configPath;
    };
    struct ModelEntry {
        QString label;
        QString path;
    };

    QString modelsDir;
    QComboBox* detectionCombo = nullptr;
    QComboBox* embeddingCombo = nullptr;
    QLabel* detectionPathLabel = nullptr;
    QLabel* embeddingPathLabel = nullptr;
    QVector<DetectionEntry> detectionEntries;
    QVector<ModelEntry> embeddingEntries;

    void buildUi();
    void populateModelLists();
    void updatePathLabels(int detectionIndex, int embeddingIndex);
    void addDetectionEntry(const QString& label, const QString& modelPath, const QString& configPath);
    void addEmbeddingEntry(const QString& label, const QString& modelPath);
    int findDetectionIndex(const QString& modelPath, const QString& configPath) const;
    int findEmbeddingIndex(const QString& modelPath) const;
    static bool isDetectionModel(const QString& fileName);
    static QString detectionLabelFor(const QString& fileName);
};

#endif // SETTINGSPAGE_H
