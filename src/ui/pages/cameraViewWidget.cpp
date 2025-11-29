/**
 * @file cameraViewWidget.cpp
 * @brief Implements the compact camera tile used within the cameras page.
 */#include "CameraViewWidget.h"
#include <QPixmap>
#include <QHBoxLayout>
#include <QDateTime>
#include <QGraphicsDropShadowEffect>

CameraViewWidget::CameraViewWidget(int id, const QString& name, QWidget* parent)
    : QWidget(parent), id(id), active(true)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // === Верхня панель ===
    auto* topBar = new QHBoxLayout;

    title = new QLabel(name);
    title->setStyleSheet("color:#bbb; font-weight:600;");

    chkAudio = new QCheckBox("🎧 Audio");
    chkAudio->setChecked(false);
    chkAudio->setStyleSheet("color:#aaa;");

    // 🔹 Індикатор Online/Offline
    statusIndicator = new QLabel;
    statusIndicator->setFixedSize(14, 14);
    statusIndicator->setStyleSheet("background:#0f0; border-radius:7px; border:1px solid #090;");

    topBar->addWidget(title);
    topBar->addStretch();
    topBar->addWidget(statusIndicator);
    topBar->addSpacing(6);
    topBar->addWidget(chkAudio);

    // === Відео ===
    preview = new QLabel("Connecting...");
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumSize(320, 200);
    preview->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    preview->setStyleSheet("background:#000; color:#666; border-radius:8px; border:1px solid #222;");

    // === Кнопка ===
    btnToggle = new QPushButton("Disable");
    btnToggle->setStyleSheet(R"(
        QPushButton {
            background:#333; color:#eee;
            border:none; padding:4px 10px;
            border-radius:4px;
        }
        QPushButton:hover { background:#444; }
        QPushButton:pressed { background:#555; }
    )");
    btnToggle->setFixedHeight(26);

    layout->addLayout(topBar);
    layout->addWidget(preview, 1, Qt::AlignCenter);
    layout->addWidget(btnToggle, 0, Qt::AlignRight);

    // === Тінь для естетики ===
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(8);
    shadow->setOffset(2, 2);
    shadow->setColor(QColor(0, 0, 0, 180));
    this->setGraphicsEffect(shadow);

    // === Зв’язки ===
    connect(btnToggle, &QPushButton::clicked, this, &CameraViewWidget::onToggleClicked);
    connect(chkAudio, &QCheckBox::toggled, this, &CameraViewWidget::onAudioToggled);

    // === Таймер перевірки активності ===
    activityTimer.setInterval(2000); // кожні 2 секунди
    connect(&activityTimer, &QTimer::timeout, this, &CameraViewWidget::checkActivity);
    activityTimer.start();

    lastFrameTime = QDateTime::currentMSecsSinceEpoch();
}

// === Кнопка Enable/Disable ===
void CameraViewWidget::onToggleClicked()
{
    active = !active;
    btnToggle->setText(active ? "Disable" : "Enable");
    preview->setText(active ? "" : "Disabled");
    preview->setStyleSheet(active
        ? "background:#000; border:1px solid #222; border-radius:8px;"
        : "background:#202020; border:1px dashed #555; border-radius:8px; color:#777;");
    emit toggleRequested(id, active);
}

// === Чекбокс Audio ===
void CameraViewWidget::onAudioToggled(bool checked)
{
    emit audioToggled(id, checked);
}

// === Кадр оновлено ===
void CameraViewWidget::updateFrame(const QImage& img)
{
    if (!active) return;

    QImage scaled = img.scaled(preview->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);

    preview->setPixmap(QPixmap::fromImage(scaled));
    preview->setText("");
    setOffline(false); // якщо отримали кадр → online
    lastFrameTime = QDateTime::currentMSecsSinceEpoch();
}

// === Позначити Offline вручну ===
void CameraViewWidget::setOffline(bool off)
{
    if (off) {
        statusIndicator->setStyleSheet("background:#f00; border-radius:7px; border:1px solid #800;");
        preview->setText("Offline");
    }
    else {
        statusIndicator->setStyleSheet("background:#0f0; border-radius:7px; border:1px solid #090;");
    }
}

// === Перевірка активності (кожні 2 сек) ===
void CameraViewWidget::checkActivity()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastFrameTime > 2000) { // 2 секунди без кадру
        setOffline(true);
    }
}

