/**
 * @file cameraViewWidget.cpp
 * @brief Implements the camera tile preview widget with controls and status updates.
 *
 * Handles UI layout, paints scaled previews, tracks heartbeat timing for online/offline
 * indication, and emits user interaction signals to the host controller.
 */
#include "cameraViewWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QDateTime>
#include <QPixmap>
#include <QResizeEvent>

CameraViewWidget::CameraViewWidget(int id, QWidget* parent)
    : QWidget(parent)
    , cameraId(id)
{
    setupUi();
    heartbeatTimer.setInterval(1000);
    connect(&heartbeatTimer, &QTimer::timeout, this, &CameraViewWidget::refreshStatus);
    heartbeatTimer.start();
}

void CameraViewWidget::setupUi()
{
    setMinimumSize(previewSize + QSize(32, 80));
    setStyleSheet(R"(
        QWidget {
            background:#101214;
            border:1px solid #1f2937;
            border-radius:8px;
        }
        QLabel#titleLabel {
            font-weight:600;
            color:#e5e7eb;
        }
        QLabel#statusLabel {
            color:#9ca3af;
            font-size:12px;
        }
        QPushButton {
            background:#2563eb;
            color:white;
            border:none;
            border-radius:4px;
            padding:4px 12px;
        }
        QPushButton:checked {
            background:#ef4444;
        }
    )");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    titleLabel = new QLabel(tr("Camera %1").arg(cameraId));
    titleLabel->setObjectName("titleLabel");

    removeButton = new QPushButton(tr("Remove"));
    removeButton->setObjectName("removeButton");

    previewLabel = new QLabel;
    previewLabel->setMinimumSize(previewSize);
    previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setStyleSheet("background:#0f172a; border-radius:6px;");

    statusLabel = new QLabel(tr("Offline"));
    statusLabel->setObjectName("statusLabel");

    toggleButton = new QPushButton(tr("Disable"));
    toggleButton->setCheckable(true);
    audioCheck = new QCheckBox(tr("Audio"));

    connect(toggleButton, &QPushButton::clicked, this, &CameraViewWidget::handleToggle);
    connect(audioCheck, &QCheckBox::toggled, this, &CameraViewWidget::handleAudio);
    connect(removeButton, &QPushButton::clicked, this, &CameraViewWidget::handleRemove);

    auto* header = new QHBoxLayout;
    header->addWidget(titleLabel);
    header->addStretch();
    header->addWidget(removeButton);

    auto* footer = new QHBoxLayout;
    footer->addWidget(statusLabel, 1);
    footer->addWidget(audioCheck);
    footer->addWidget(toggleButton);

    layout->addLayout(header);
    layout->addWidget(previewLabel, 1);
    layout->addLayout(footer);
}

void CameraViewWidget::setTitle(const QString& text)
{
    titleLabel->setText(text);
}

void CameraViewWidget::updateFrame(const QImage& image)
{
    if (image.isNull())
        return;

    lastFrameMs = QDateTime::currentMSecsSinceEpoch();
    lastFrame = image;
    if (cameraEnabled)
        updatePreviewPixmap();
}

void CameraViewWidget::setStatusText(const QString& text)
{
    statusLabel->setText(text);
}

void CameraViewWidget::setOnline(bool value)
{
    online = value;
    if (!online) {
        previewLabel->setPixmap(QPixmap());
        lastFrame = QImage();
        statusLabel->setText(tr("Offline"));
    }
}

void CameraViewWidget::setAudioChecked(bool enabled)
{
    audioCheck->setChecked(enabled);
}

void CameraViewWidget::setCameraActive(bool enable)
{
    cameraEnabled = enable;
    toggleButton->setChecked(!enable);
    toggleButton->setText(enable ? tr("Disable") : tr("Enable"));
    previewLabel->setEnabled(enable);
    audioCheck->setEnabled(enable && audioCheck->isVisible());
    if (!enable) {
        previewLabel->setPixmap(QPixmap());
        statusLabel->setText(tr("Disabled"));
    } else if (!lastFrame.isNull()) {
        updatePreviewPixmap();
    }
}

void CameraViewWidget::setAudioVisible(bool visible)
{
    audioCheck->setVisible(visible);
    if (!visible) {
        audioCheck->setChecked(false);
        audioCheck->setEnabled(false);
    } else if (cameraEnabled) {
        audioCheck->setEnabled(true);
    }
}

void CameraViewWidget::handleToggle()
{
    emit toggleRequested(cameraId, !cameraEnabled);
}

void CameraViewWidget::handleAudio(bool checked)
{
    emit audioToggled(cameraId, checked);
}

void CameraViewWidget::handleRemove()
{
    emit removeRequested(cameraId);
}

void CameraViewWidget::refreshStatus()
{
    if (!cameraEnabled) {
        statusLabel->setText(tr("Disabled"));
        return;
    }

    if (lastFrameMs == 0)
        return;

    const qint64 diff = QDateTime::currentMSecsSinceEpoch() - lastFrameMs;
    if (diff > 5000) {
        setOnline(false);
    } else {
        setOnline(true);
        statusLabel->setText(tr("Online (%1 ms ago)").arg(diff));
    }
}

void CameraViewWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updatePreviewPixmap();
}

void CameraViewWidget::updatePreviewPixmap()
{
    if (!cameraEnabled || lastFrame.isNull() || previewLabel->size().isEmpty())
        return;

    const QImage scaled = lastFrame.scaled(
        previewLabel->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    previewLabel->setPixmap(QPixmap::fromImage(scaled));
}
