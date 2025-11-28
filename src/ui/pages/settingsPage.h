#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QVector>

class QComboBox;
class QPushButton;
class QLabel;

class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(const QString& modelsDirectory, QWidget* parent = nullptr);

    void setCurrentModels(const QString& detectionModel, const QString& detectionConfig, const QString& embeddingModel, const QString& genderModel);

signals:
    void detectionModelSelected(const QString& modelPath, const QString& configPath);
    void embeddingModelSelected(const QString& modelPath);
    void genderModelSelected(const QString& modelPath);

private slots:
    void onDetectionChanged(int index);
    void onEmbeddingChanged(int index);
    void onGenderChanged(int index);
    void onBrowseDetection();
    void onBrowseEmbedding();
    void onBrowseGender();

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
    QComboBox* genderCombo = nullptr;
    QLabel* detectionPathLabel = nullptr;
    QLabel* embeddingPathLabel = nullptr;
    QLabel* genderPathLabel = nullptr;
    QVector<DetectionEntry> detectionEntries;
    QVector<ModelEntry> embeddingEntries;
    QVector<ModelEntry> genderEntries;

    void buildUi();
    void populateModelLists();
    void updatePathLabels(int detectionIndex, int embeddingIndex, int genderIndex);
    void addDetectionEntry(const QString& label, const QString& modelPath, const QString& configPath);
    void addEmbeddingEntry(const QString& label, const QString& modelPath);
    void addGenderEntry(const QString& label, const QString& modelPath);
    int findDetectionIndex(const QString& modelPath, const QString& configPath) const;
    int findEmbeddingIndex(const QString& modelPath) const;
    int findGenderIndex(const QString& modelPath) const;
    static bool isDetectionModel(const QString& fileName);
    static QString detectionLabelFor(const QString& fileName);
    static bool isGenderModel(const QString& fileName);
};

#endif // SETTINGSPAGE_H
