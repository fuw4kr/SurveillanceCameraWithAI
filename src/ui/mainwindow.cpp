/**
 * @file MainWindow.cpp
 * @brief Implementation of MainWindow (no .ui file).
 */

#include "MainWindow.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QStyle>
#include <QCoreApplication>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QMetaObject>
#include <QStringList>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : FramelessWindow(parent)
{
    setupUi();
    setupTitleBar();
    setupSettingsPopup();
    setupSidebar();
    setupConsole();
    setupConnections();

    setWindowTitle("AI Smart Surveillance System");
    resize(1200, 800);

    snapPreview = new SnapPreviewWindow(this);
    connect(this, &FramelessWindow::windowMaximizedChanged, this, &MainWindow::updateMaximizeIcon);
}

MainWindow::~MainWindow()
{
    if (aiThread) {
        aiThread->quit();
        aiThread->wait();
        aiThread->deleteLater();
        aiThread = nullptr;
        aiProcessor = nullptr;
    }

    if (snapPreview) {
        snapPreview->hidePreview();
        snapPreview->deleteLater();
    }
}

// === UI building ===
void MainWindow::setupUi()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ==== title bar ====
    titleBar = new QWidget;
    titleBar->setObjectName("titleBar");
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(8, 0, 8, 0);
    titleLayout->setSpacing(6);

    IconName = new QLabel("??");
    titleLabel = new QLabel("AI Smart Surveillance System");
    titleLabel->setStyleSheet("font-weight:600; color:#ccc;");
    btnSettings = new QPushButton(QStringLiteral("\u2699"));
    btnMinimize = new QPushButton("-");
    btnMaximize = new QPushButton("?");
    btnClose = new QPushButton("?");
    btnClose->setObjectName("btnClose");
    btnSettings->setObjectName("btnSettings");

    for (auto* b : { btnSettings, btnMinimize, btnMaximize, btnClose }) {
        b->setFixedSize(32, 28);
        b->setFlat(true);
    }
    btnSettings->setToolTip(tr("Settings"));

    titleLayout->addWidget(IconName);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(btnSettings);
    titleLayout->addWidget(btnMinimize);
    titleLayout->addWidget(btnMaximize);
    titleLayout->addWidget(btnClose);

    // ==== content ====
    QHBoxLayout* centerLayout = new QHBoxLayout;
    centerLayout->setContentsMargins(4, 4, 4, 4);
    centerLayout->setSpacing(4);

    listModes = new QListWidget;
    listModes->addItems({
        "?? Dashboard",
        "?? Live Cameras",
        "?? Heatmap Analytics",
        "?? Events / Logs",
        "?? 3D Face Viewer",
        "?? AI Analytics",
        "?? System Console",
        "?? Settings"
        });
    listModes->setFixedWidth(220);

    stackedWidget = new QStackedWidget;
    stackedWidget->setObjectName("stackedWidget");

    auto dashboard = new DashboardPage;
    cameraManager = new CameraManager(this);
    aiProcessor = new AIProcessor();

    const auto pickPath = [](const QStringList& candidates) -> QString {
        for (const auto& path : candidates) {
            if (QFileInfo::exists(path))
                return path;
        }
        return {};
    };

    const QString binDir = QCoreApplication::applicationDirPath();
    const QString faceModel = pickPath({
        binDir + "/assets/models/res10_300x300_ssd_iter_140000.caffemodel",
        binDir + "/../assets/models/res10_300x300_ssd_iter_140000.caffemodel",
        binDir + "/../../assets/models/res10_300x300_ssd_iter_140000.caffemodel",
        binDir + "/../../../assets/models/res10_300x300_ssd_iter_140000.caffemodel"
    });
    const QString faceConfig = pickPath({
        binDir + "/assets/models/deploy.prototxt",
        binDir + "/../assets/models/deploy.prototxt",
        binDir + "/../../assets/models/deploy.prototxt",
        binDir + "/../../../assets/models/deploy.prototxt"
    });
    if (!faceModel.isEmpty() && !faceConfig.isEmpty()) {
        qInfo() << "Loading face model from" << faceModel << "and" << faceConfig;
        aiProcessor->loadFaceModel(faceModel, faceConfig);
    } else {
        qWarning() << "Face model files not found; DNN face detection disabled";
    }

    const QString embedModel = pickPath({
        binDir + "/assets/models/face_embedding.onnx",
        binDir + "/assets/models/mobilefacenet.onnx",
        binDir + "/assets/models/arcface_r100.onnx",
        binDir + "/assets/models/arcface.onnx",
        binDir + "/../assets/models/face_embedding.onnx",
        binDir + "/../assets/models/mobilefacenet.onnx",
        binDir + "/../assets/models/arcface_r100.onnx",
        binDir + "/../assets/models/arcface.onnx",
        binDir + "/../../assets/models/face_embedding.onnx",
        binDir + "/../../assets/models/mobilefacenet.onnx",
        binDir + "/../../assets/models/arcface_r100.onnx",
        binDir + "/../../assets/models/arcface.onnx",
        binDir + "/../../../assets/models/face_embedding.onnx",
        binDir + "/../../../assets/models/mobilefacenet.onnx",
        binDir + "/../../../assets/models/arcface_r100.onnx",
        binDir + "/../../../assets/models/arcface.onnx"
    });
    if (!embedModel.isEmpty()) {
        qInfo() << "Loading embedding model from" << embedModel;
        aiProcessor->loadEmbedModel(embedModel);
        embedModelPath = embedModel;
    } else {
        qWarning() << "Embedding model (.onnx) not found in assets/models; face recognition disabled";
    }
    cachedRecognitionInterval = aiProcessor ? aiProcessor->recognitionInterval() : cachedRecognitionInterval;
    cachedGpuPreference = aiProcessor ? aiProcessor->prefersGpuForEmbeddings() : cachedGpuPreference;

    aiThread = new QThread(this);
    aiProcessor->moveToThread(aiThread);
    connect(aiThread, &QThread::finished, aiProcessor, &QObject::deleteLater);
    aiThread->start();

    camerasPage = new CamerasPage(cameraManager, aiProcessor, this);

    stackedWidget->addWidget(dashboard);
    stackedWidget->addWidget(camerasPage);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);

    centerLayout->addWidget(listModes);
    centerLayout->addWidget(stackedWidget, 1);

    consoleView = new QListView;
    consoleView->setFixedHeight(180);

    mainLayout->addWidget(titleBar);
    mainLayout->addLayout(centerLayout, 1);
    mainLayout->addWidget(consoleView);

    setCentralWidget(central);

    connect(listModes, &QListWidget::currentRowChanged, this, [=](int index) {
        stackedWidget->setCurrentIndex(index);
    });

    listModes->setCurrentRow(0);
}

// === Title bar ===
void MainWindow::setupTitleBar()
{
    titleBar->setMinimumHeight(36);
    titleBar->setMaximumHeight(36);
    titleBar->setStyleSheet(R"(
        QWidget#titleBar {
            background:#1E1E1E;
            border-bottom:1px solid #333;
        }
        QPushButton {
            color:#ccc;
            border:none;
            font-size:14px;
        }
        QPushButton:hover {
            background:#333;
        }
        QPushButton#btnClose:hover {
            background:#500;
            color:#ff5555;
        }
    )");

    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QPushButton::clicked, this, &MainWindow::toggleMaximizeRestore);
}

void MainWindow::setupSettingsPopup()
{
    settingsPopup = new QWidget(this, Qt::Popup);
    settingsPopup->setObjectName("settingsPopup");
    settingsPopup->setStyleSheet(R"(
        QWidget#settingsPopup {
            background:#1f1f1f;
            border:1px solid #333;
            border-radius:6px;
        }
        QLabel { color:#ddd; }
        QCheckBox { color:#ddd; }
    )");

    QVBoxLayout* popupLayout = new QVBoxLayout(settingsPopup);
    popupLayout->setContentsMargins(12, 12, 12, 12);
    popupLayout->setSpacing(8);

    QLabel* recognitionLabel = new QLabel(tr("Face recognition interval (ms)"), settingsPopup);
    recognitionLabel->setStyleSheet("font-weight:600;");
    recognitionSlider = new QSlider(Qt::Horizontal, settingsPopup);
    recognitionSlider->setRange(100, 3000);
    recognitionSlider->setSingleStep(50);
    recognitionSlider->setPageStep(100);
    recognitionValueLabel = new QLabel(settingsPopup);
    recognitionValueLabel->setStyleSheet("color:#aaa;");

    gpuToggle = new QCheckBox(tr("Use GPU acceleration (OpenVINO)"), settingsPopup);
    recognitionSlider->setValue(cachedRecognitionInterval);
    recognitionValueLabel->setText(QStringLiteral("%1 ms").arg(cachedRecognitionInterval));
    gpuToggle->setChecked(cachedGpuPreference);

    popupLayout->addWidget(recognitionLabel);
    popupLayout->addWidget(recognitionSlider);
    popupLayout->addWidget(recognitionValueLabel);
    popupLayout->addSpacing(4);
    popupLayout->addWidget(gpuToggle);

    refreshSettingsUi();
}

// === Sidebar ===
void MainWindow::setupSidebar()
{
    listModes->setSelectionMode(QAbstractItemView::SingleSelection);
    listModes->setCurrentRow(0);
    listModes->setStyleSheet(R"(
        QListWidget {
            background:#141414; color:#ccc; border:none;
        }
        QListWidget::item { padding:8px 12px; }
        QListWidget::item:selected {
            background:#2563EB; color:white;
            border-left:3px solid #3B82F6;
        }
    )");
}

// === Console ===
void MainWindow::setupConsole()
{
    consoleView->setStyleSheet(R"(
        QListView {
            background:#0d0d0d;
            color:#00FF6E;
            font-family:Consolas;
            font-size:13px;
            border-top:1px solid #222;
        }
    )");
}

// === Connections ===
void MainWindow::setupConnections()
{
    connect(listModes, &QListWidget::currentRowChanged,
        this, &MainWindow::onModeChanged);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::toggleSettingsPopup);
    if (recognitionSlider)
        connect(recognitionSlider, &QSlider::valueChanged, this, &MainWindow::handleRecognitionSlider);
    if (gpuToggle)
        connect(gpuToggle, &QCheckBox::toggled, this, &MainWindow::handleGpuToggle);

    if (aiProcessor) {
        connect(aiProcessor, &AIProcessor::recognitionIntervalChanged, this, [this](int interval) {
            cachedRecognitionInterval = interval;
            refreshSettingsUi();
        }, Qt::QueuedConnection);
        connect(aiProcessor, &AIProcessor::embeddingBackendChanged, this, [this](bool useGpu) {
            cachedGpuPreference = useGpu;
            refreshSettingsUi();
        }, Qt::QueuedConnection);
    }
}

// === Slots ===
void MainWindow::onModeChanged(int index)
{
    stackedWidget->setCurrentIndex(index);
    QString mode = listModes->item(index)->text();
    qDebug() << "Mode changed to:" << mode;
}

void MainWindow::updateMaximizeIcon(bool maxed)
{
    bool isLight = true;
    QString path;
    if (maxed)
        path = isLight
        ? ":/resources/icons/icons-for-window/minimize-black.png"
        : ":/resources/icons/icons-for-window/minimize-white.png";
    else
        path = isLight
        ? ":/resources/icons/icons-for-window/maximize-black.png"
        : ":/resources/icons/icons-for-window/maximize-white.png";
    btnMaximize->setIcon(QIcon(path));
}

void MainWindow::toggleSettingsPopup()
{
    if (!settingsPopup || !btnSettings)
        return;

    if (settingsPopup->isVisible()) {
        settingsPopup->hide();
        return;
    }

    refreshSettingsUi();
    QSize popupSize = settingsPopup->sizeHint();
    QPoint globalPos = btnSettings->mapToGlobal(QPoint(btnSettings->width() - popupSize.width(), btnSettings->height()));
    globalPos.setX(std::max(0, globalPos.x()));
    globalPos.setY(std::max(0, globalPos.y()));
    settingsPopup->resize(popupSize);
    settingsPopup->move(globalPos);
    settingsPopup->show();
}

void MainWindow::handleRecognitionSlider(int value)
{
    cachedRecognitionInterval = value;
    if (recognitionValueLabel)
        recognitionValueLabel->setText(QStringLiteral("%1 ms").arg(value));
    if (aiProcessor)
        QMetaObject::invokeMethod(aiProcessor, "setRecognitionIntervalMs", Qt::QueuedConnection, Q_ARG(int, value));
}

void MainWindow::handleGpuToggle(bool checked)
{
    cachedGpuPreference = checked;
    if (!aiProcessor)
        return;
    QMetaObject::invokeMethod(aiProcessor, "setPreferGpuForEmbeddings", Qt::QueuedConnection, Q_ARG(bool, checked));
    if (!embedModelPath.isEmpty()) {
        const QString modelPath = embedModelPath;
        QMetaObject::invokeMethod(aiProcessor, "loadEmbedModelAsync", Qt::QueuedConnection, Q_ARG(QString, modelPath));
    }
}

void MainWindow::refreshSettingsUi()
{
    if (recognitionSlider) {
        const int interval = std::clamp(cachedRecognitionInterval, recognitionSlider->minimum(), recognitionSlider->maximum());
        const QSignalBlocker blocker(recognitionSlider);
        recognitionSlider->setValue(interval);
        if (recognitionValueLabel)
            recognitionValueLabel->setText(QStringLiteral("%1 ms").arg(interval));
    }

    if (gpuToggle) {
        const QSignalBlocker blocker(gpuToggle);
        gpuToggle->setChecked(cachedGpuPreference);
    }
}
