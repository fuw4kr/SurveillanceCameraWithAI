/**
 * @file settingsPage.cpp
 * @brief Implements the model selection page for detection and embeddings.
 */
#include "settingsPage.h"

#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QSignalBlocker>
#include <QStringList>

namespace {
QString normalized(const QString& path)
{
    QString normalizedPath = path;
    normalizedPath.replace('\\', '/');
    return normalizedPath;
}
}

SettingsPage::SettingsPage(const QString& modelsDirectory, QWidget* parent)
    : QWidget(parent)
    , modelsDir(modelsDirectory)
{
    buildUi();
    populateModelLists();
}

void SettingsPage::setCurrentModels(const QString& detectionModel, const QString& detectionConfig, const QString& embeddingModel)
{
    QSignalBlocker blockerDet(detectionCombo);
    QSignalBlocker blockerEmb(embeddingCombo);

    const int detIndex = findDetectionIndex(detectionModel, detectionConfig);
    if (detIndex >= 0)
        detectionCombo->setCurrentIndex(detIndex);

    const int embIndex = findEmbeddingIndex(embeddingModel);
    if (embIndex >= 0)
        embeddingCombo->setCurrentIndex(embIndex);

    updatePathLabels(detectionCombo->currentIndex(), embeddingCombo->currentIndex());
}

void SettingsPage::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(24);

    auto* faceGroup = new QGroupBox(tr("Face Detection"));
    auto* faceLayout = new QFormLayout(faceGroup);
    faceLayout->setLabelAlignment(Qt::AlignLeft);

    detectionCombo = new QComboBox;
    detectionCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(detectionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SettingsPage::onDetectionChanged);

    auto* detRow = new QHBoxLayout;
    detRow->addWidget(detectionCombo, 1);
    auto* browseDetection = new QPushButton(tr("Browse..."));
    connect(browseDetection, &QPushButton::clicked, this, &SettingsPage::onBrowseDetection);
    detRow->addWidget(browseDetection);
    faceLayout->addRow(tr("Model"), detRow);

    detectionPathLabel = new QLabel;
    detectionPathLabel->setObjectName(QStringLiteral("detectionPathLabel"));
    detectionPathLabel->setStyleSheet(QStringLiteral("color:#8b949e; font-size:12px;"));
    detectionPathLabel->setWordWrap(true);
    faceLayout->addRow(tr("Path"), detectionPathLabel);

    auto* embedGroup = new QGroupBox(tr("Face Recognition / Embedding"));
    auto* embedLayout = new QFormLayout(embedGroup);

    embeddingCombo = new QComboBox;
    embeddingCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(embeddingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &SettingsPage::onEmbeddingChanged);

    auto* embRow = new QHBoxLayout;
    embRow->addWidget(embeddingCombo, 1);
    auto* browseEmbedding = new QPushButton(tr("Browse..."));
    connect(browseEmbedding, &QPushButton::clicked, this, &SettingsPage::onBrowseEmbedding);
    embRow->addWidget(browseEmbedding);
    embedLayout->addRow(tr("Model"), embRow);

    embeddingPathLabel = new QLabel;
    embeddingPathLabel->setStyleSheet(QStringLiteral("color:#8b949e; font-size:12px;"));
    embeddingPathLabel->setWordWrap(true);
    embedLayout->addRow(tr("Path"), embeddingPathLabel);

    layout->addWidget(faceGroup);
    layout->addWidget(embedGroup);
    layout->addStretch();
}

void SettingsPage::populateModelLists()
{
    detectionEntries.clear();
    embeddingEntries.clear();
    detectionCombo->clear();
    embeddingCombo->clear();

    QDir dir(modelsDir);
    if (!dir.exists())
        dir = QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/assets/models"));

    QDirIterator it(dir.absolutePath(), QStringList() << "*.onnx"
                                                      << "*.caffemodel",
        QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QFileInfo info(it.next());
        const QString fileNameLower = info.fileName().toLower();
        if (fileNameLower.contains(QStringLiteral("genderage")))
            continue;
        const QString absPath = normalized(info.absoluteFilePath());
        if (isDetectionModel(fileNameLower)) {
            QString configPath;
            if (info.suffix().compare(QStringLiteral("caffemodel"), Qt::CaseInsensitive) == 0) {
                const QString paired = info.absolutePath() + QStringLiteral("/") + info.completeBaseName() + QStringLiteral(".prototxt");
                if (QFileInfo::exists(paired))
                    configPath = normalized(paired);
                else {
                    const QString deployPath = info.absolutePath() + QStringLiteral("/deploy.prototxt");
                    if (QFileInfo::exists(deployPath))
                        configPath = normalized(deployPath);
                }
                if (configPath.isEmpty())
                    continue;
            }
            addDetectionEntry(detectionLabelFor(info.fileName()), absPath, normalized(configPath));
        } else {
            addEmbeddingEntry(info.completeBaseName(), absPath);
        }
    }

    if (detectionEntries.isEmpty())
        detectionCombo->addItem(tr("No face detectors found"));
    if (embeddingEntries.isEmpty())
        embeddingCombo->addItem(tr("No embedding models found"));
}

void SettingsPage::onDetectionChanged(int index)
{
    if (index < 0 || index >= detectionEntries.size())
        return;
    const auto& entry = detectionEntries.at(index);
    updatePathLabels(index, embeddingCombo->currentIndex());
    emit detectionModelSelected(entry.modelPath, entry.configPath);
}

void SettingsPage::onEmbeddingChanged(int index)
{
    if (index < 0 || index >= embeddingEntries.size())
        return;
    const auto& entry = embeddingEntries.at(index);
    updatePathLabels(detectionCombo->currentIndex(), index);
    emit embeddingModelSelected(entry.path);
}

void SettingsPage::onBrowseDetection()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select Face Detection Model"),
        modelsDir, tr("Models (*.onnx *.caffemodel);;All Files (*.*)"));
    if (file.isEmpty())
        return;

    QString config;
    QFileInfo info(file);
    if (info.suffix().compare(QStringLiteral("caffemodel"), Qt::CaseInsensitive) == 0) {
        const QString configFile = QFileDialog::getOpenFileName(this, tr("Select Prototxt for Caffe model"),
            info.absolutePath(), tr("Prototxt (*.prototxt);;All Files (*.*)"));
        if (configFile.isEmpty())
            return;
        config = normalized(configFile);
    }

    const QString modelPath = normalized(file);
    addDetectionEntry(detectionLabelFor(info.fileName()), modelPath, normalized(config));
    const int newIndex = detectionEntries.size() - 1;
    detectionCombo->setCurrentIndex(newIndex);
    emit detectionModelSelected(modelPath, config);
}

void SettingsPage::onBrowseEmbedding()
{
    const QString file = QFileDialog::getOpenFileName(this, tr("Select Embedding Model"),
        modelsDir, tr("ONNX Models (*.onnx);;All Files (*.*)"));
    if (file.isEmpty())
        return;
    const QString modelPath = normalized(file);
    addEmbeddingEntry(QFileInfo(file).completeBaseName(), modelPath);
    const int newIndex = embeddingEntries.size() - 1;
    embeddingCombo->setCurrentIndex(newIndex);
    emit embeddingModelSelected(modelPath);
}

void SettingsPage::updatePathLabels(int detectionIndex, int embeddingIndex)
{
    if (detectionIndex >= 0 && detectionIndex < detectionEntries.size()) {
        const auto& entry = detectionEntries.at(detectionIndex);
        QString text = entry.modelPath;
        if (!entry.configPath.isEmpty())
            text += QStringLiteral("\n") + entry.configPath;
        detectionPathLabel->setText(text);
    } else {
        detectionPathLabel->clear();
    }

    if (embeddingIndex >= 0 && embeddingIndex < embeddingEntries.size())
        embeddingPathLabel->setText(embeddingEntries.at(embeddingIndex).path);
    else
        embeddingPathLabel->clear();
}

void SettingsPage::addDetectionEntry(const QString& label, const QString& modelPath, const QString& configPath)
{
    if (findDetectionIndex(modelPath, configPath) >= 0)
        return;
    DetectionEntry entry { label, modelPath, configPath };
    detectionEntries.append(entry);
    detectionCombo->addItem(label);
}

void SettingsPage::addEmbeddingEntry(const QString& label, const QString& modelPath)
{
    if (findEmbeddingIndex(modelPath) >= 0)
        return;
    ModelEntry entry { label, modelPath };
    embeddingEntries.append(entry);
    embeddingCombo->addItem(label);
}

int SettingsPage::findDetectionIndex(const QString& modelPath, const QString& configPath) const
{
    const QString normalizedModel = normalized(modelPath);
    const QString normalizedConfig = normalized(configPath);
    for (int i = 0; i < detectionEntries.size(); ++i) {
        if (normalized(detectionEntries[i].modelPath) == normalizedModel
            && normalized(detectionEntries[i].configPath) == normalizedConfig)
            return i;
    }
    return -1;
}

int SettingsPage::findEmbeddingIndex(const QString& modelPath) const
{
    const QString normalizedModel = normalized(modelPath);
    for (int i = 0; i < embeddingEntries.size(); ++i) {
        if (normalized(embeddingEntries[i].path) == normalizedModel)
            return i;
    }
    return -1;
}

bool SettingsPage::isDetectionModel(const QString& fileName)
{
    const QString lower = fileName.toLower();
    if (lower.endsWith(QStringLiteral(".caffemodel")))
        return true;
    if (!lower.endsWith(QStringLiteral(".onnx")))
        return false;
    static const QStringList keywords = { QStringLiteral("det"), QStringLiteral("face"), QStringLiteral("yunet"), QStringLiteral("scrfd") };
    for (const QString& keyword : keywords) {
        if (lower.contains(keyword))
            return true;
    }
    return false;
}

QString SettingsPage::detectionLabelFor(const QString& fileName)
{
    QFileInfo info(fileName);
    return info.completeBaseName();
}
