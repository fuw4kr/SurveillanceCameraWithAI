#include "analyticsPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPainter>
#include <QImage>
#include <QPixmap>
#include <QIcon>

#include <opencv2/imgproc.hpp>

AnalyticsPage::AnalyticsPage(CameraManager* manager, AIProcessor* processor, QWidget* parent)
    : QWidget(parent)
    , cameraManager(manager)
    , aiProcessor(processor)
{
    buildUi();

    if (cameraManager) {
        connect(cameraManager, &CameraManager::camerasChanged,
            this, &AnalyticsPage::refreshCameras);
        connect(cameraManager, &CameraManager::frameReady,
            this, &AnalyticsPage::handleFrame, Qt::QueuedConnection);
    }

    refreshCameras();
}

void AnalyticsPage::buildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(16);

    auto* header = new QHBoxLayout;
    auto* title = new QLabel(tr("AI Analytics"));
    title->setStyleSheet("font-size:20px; font-weight:600;");

    cameraCombo = new QComboBox;
    cameraCombo->setMinimumWidth(200);

    connect(cameraCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AnalyticsPage::onCameraChanged);

    header->addWidget(title);
    header->addStretch();
    header->addWidget(new QLabel(tr("Camera:")));
    header->addWidget(cameraCombo);

    previewLabel = new QLabel;
    previewLabel->setMinimumSize(640, 360);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet("background:#050505; border:1px solid #1f2937; border-radius:8px;");
    previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    statsLabel = new QLabel(tr("No analytics data"));
    statsLabel->setStyleSheet("color:#e5e7eb; font-weight:500;");

    detectionList = new QListWidget;
    detectionList->setMinimumWidth(260);
    detectionList->setIconSize(QSize(64, 64));
    detectionList->setStyleSheet(R"(
        QListWidget {
            background:#111827;
            border:1px solid #1f2937;
            border-radius:6px;
            color:#e5e7eb;
        }
    )");

    auto* contentLayout = new QHBoxLayout;
    contentLayout->addWidget(previewLabel, 1);
    contentLayout->addWidget(detectionList);

    layout->addLayout(header);
    layout->addWidget(statsLabel);
    layout->addLayout(contentLayout, 1);
}

void AnalyticsPage::refreshCameras()
{
    if (!cameraManager)
        return;

    const QList<int> ids = cameraManager->ids();
    const int previousId = currentCameraId;

    cameraCombo->blockSignals(true);
    cameraCombo->clear();
    for (int id : ids) {
        const auto descriptor = cameraManager->descriptor(id);
        const QString text = QStringLiteral("%1 (%2)").arg(descriptor.name).arg(id);
        cameraCombo->addItem(text, id);
    }
    cameraCombo->blockSignals(false);

    if (ids.isEmpty()) {
        currentCameraId = -1;
        cameraCombo->setCurrentIndex(-1);
        statsLabel->setText(tr("No cameras configured"));
        previewLabel->setPixmap(QPixmap());
        detectionList->clear();
        return;
    }

    if (!ids.contains(previousId))
        currentCameraId = ids.first();

    const int comboIndex = cameraCombo->findData(currentCameraId);
    cameraCombo->setCurrentIndex(comboIndex >= 0 ? comboIndex : 0);
}

void AnalyticsPage::onCameraChanged(int index)
{
    currentCameraId = cameraCombo->itemData(index).toInt();
    if (aiProcessor)
        aiProcessor->resetBackground();
    detectionList->clear();
    statsLabel->setText(tr("Waiting for frames..."));
}

void AnalyticsPage::handleFrame(int id, const QImage& image)
{
    if (id != currentCameraId || !aiProcessor)
        return;

    const cv::Mat mat = imageToMat(image);
    if (mat.empty())
        return;

    ProcessedFrame processed = aiProcessor->processFrame(mat, id);
    if (!processed.annotated.empty()) {
        const QImage annotated = matToImage(processed.annotated);
        previewLabel->setPixmap(QPixmap::fromImage(annotated).scaled(
            previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        previewLabel->setPixmap(QPixmap::fromImage(image).scaled(
            previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    updateDetections(processed.detections);
}

void AnalyticsPage::updateDetections(const QVector<Detection>& detections)
{
    detectionList->clear();
    for (const Detection& d : detections) {
        const QString displayLabel = d.label.isEmpty() ? d.category : d.label;
        const QString entry = QString("%1 [%2]  |  conf:%3  |  (%4,%5,%6,%7)")
                                  .arg(displayLabel)
                                  .arg(d.category.isEmpty() ? tr("Unknown") : d.category)
                                  .arg(QString::number(d.confidence, 'f', 2))
                                  .arg(QString::number(d.rect.x()))
                                  .arg(QString::number(d.rect.y()))
                                  .arg(QString::number(d.rect.width()))
                                  .arg(QString::number(d.rect.height()));

        auto* item = new QListWidgetItem(entry);
        if (!d.previewPath.isEmpty()) {
            QPixmap pixmap(d.previewPath);
            if (!pixmap.isNull()) {
                const QPixmap thumb = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                item->setIcon(QIcon(thumb));
            }
        }
        detectionList->addItem(item);
    }

    if (detections.isEmpty())
        statsLabel->setText(tr("No detections"));
    else
        updateStats(detections);
}

void AnalyticsPage::updateStats(const QVector<Detection>& detections)
{
    int faceCount = 0;
    int objectCount = 0;
    int personCount = 0;
    for (const Detection& d : detections) {
        const QString category = d.category.isEmpty() ? d.label : d.category;
        if (category.compare("Face", Qt::CaseInsensitive) == 0)
            ++faceCount;
        else if (category.compare("Object", Qt::CaseInsensitive) == 0)
            ++objectCount;
        else if (category.compare("Person", Qt::CaseInsensitive) == 0)
            ++personCount;
    }

    statsLabel->setText(tr("Faces: %1   Persons: %2   Objects: %3")
                            .arg(faceCount)
                            .arg(personCount)
                            .arg(objectCount));
}

cv::Mat AnalyticsPage::imageToMat(const QImage& image) const
{
    if (image.isNull())
        return {};

    QImage converted = image.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(converted.height(), converted.width(), CV_8UC3,
        const_cast<uchar*>(converted.bits()), converted.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

QImage AnalyticsPage::matToImage(const cv::Mat& mat) const
{
    if (mat.empty())
        return {};

    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}
