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
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QSignalBlocker>
#include <QMetaObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : FramelessWindow(parent)
{
    setupUi();
    setupTitleBar();
    setupSettingsPopup();
    setupSidebar();
    setupConsole();
    loadThemeStyles();
    applyTheme(currentTheme);
    setupConnections();
    setupDashboardPolling();

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

    IconName = new QLabel("AI");
    IconName->setObjectName("IconName");
    titleLabel = new QLabel("AI Smart Surveillance System");
    titleLabel->setObjectName("titleLabel");
    btnThemeToggle = new QPushButton(tr("Dark"));
    btnThemeToggle->setObjectName("btnThemeToggle");
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
    btnThemeToggle->setFixedHeight(28);
    btnThemeToggle->setMinimumWidth(72);
    btnThemeToggle->setFlat(true);
    btnSettings->setToolTip(tr("Settings"));

    titleLayout->addWidget(IconName);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(btnThemeToggle);
    titleLayout->addWidget(btnSettings);
    titleLayout->addWidget(btnMinimize);
    titleLayout->addWidget(btnMaximize);
    titleLayout->addWidget(btnClose);

    // ==== content ====
    QHBoxLayout* centerLayout = new QHBoxLayout;
    centerLayout->setContentsMargins(4, 4, 4, 4);
    centerLayout->setSpacing(4);

    listModes = new QListWidget;
    listModes->setObjectName("listModes");
    listModes->addItems({
        "?? Dashboard",
        "?? Live Cameras",
        "?? Face Database",
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

    dashboardPage = new DashboardPage;
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
    analyticsPage = new AnalyticsPage(cameraManager, aiProcessor, this);
    faceDbPage = new FaceDatabasePage(aiProcessor, this);

    stackedWidget->addWidget(dashboardPage);
    stackedWidget->addWidget(camerasPage);
    stackedWidget->addWidget(faceDbPage);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(analyticsPage);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);

    centerLayout->addWidget(listModes);
    centerLayout->addWidget(stackedWidget, 1);

    consoleView = new QListView;
    consoleView->setObjectName("consoleView");
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

    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QPushButton::clicked, this, &MainWindow::toggleMaximizeRestore);
}

void MainWindow::setupSettingsPopup()
{
    settingsPopup = new QWidget(this, Qt::Popup);
    settingsPopup->setObjectName("settingsPopup");

    QVBoxLayout* popupLayout = new QVBoxLayout(settingsPopup);
    popupLayout->setContentsMargins(12, 12, 12, 12);
    popupLayout->setSpacing(8);

    QLabel* recognitionLabel = new QLabel(tr("Face recognition interval (ms)"), settingsPopup);
    recognitionLabel->setObjectName("recognitionLabel");
    recognitionSlider = new QSlider(Qt::Horizontal, settingsPopup);
    recognitionSlider->setRange(100, 3000);
    recognitionSlider->setSingleStep(50);
    recognitionSlider->setPageStep(100);
    recognitionValueLabel = new QLabel(settingsPopup);
    recognitionValueLabel->setObjectName("recognitionValueLabel");

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
}

// === Console ===
void MainWindow::setupConsole()
{
}

// === Connections ===
void MainWindow::setupConnections()
{
    connect(listModes, &QListWidget::currentRowChanged,
        this, &MainWindow::onModeChanged);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::toggleSettingsPopup);
    connect(btnThemeToggle, &QPushButton::clicked, this, &MainWindow::toggleTheme);
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

void MainWindow::toggleTheme()
{
    const Theme nextTheme = (currentTheme == Theme::Light) ? Theme::Dark : Theme::Light;
    applyTheme(nextTheme);
}

void MainWindow::applyTheme(Theme theme)
{
    currentTheme = theme;
    const QString stylesheet = (theme == Theme::Light) ? lightStylesheet : darkStylesheet;
    if (!stylesheet.isEmpty()) {
        qApp->setStyleSheet(stylesheet);
    } else {
        qApp->setStyleSheet(QString());
    }

    if (btnThemeToggle) {
        if (theme == Theme::Light) {
            btnThemeToggle->setText(tr("Light"));
            btnThemeToggle->setToolTip(tr("Switch to dark theme"));
        } else {
            btnThemeToggle->setText(tr("Dark"));
            btnThemeToggle->setToolTip(tr("Switch to light theme"));
        }
    }

    updateMaximizeIcon(isMaximized);
}

void MainWindow::loadThemeStyles()
{
    lightStylesheet = loadStylesheet(":/resources/styles/light.qss");
    darkStylesheet = loadStylesheet(":/resources/styles/dark.qss");
    if (lightStylesheet.isEmpty() || darkStylesheet.isEmpty()) {
        qWarning() << "Theme stylesheets are missing or empty; theme switching may look incorrect.";
    }
}

QString MainWindow::loadStylesheet(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open stylesheet:" << path << "-" << file.errorString();
        return {};
    }
    return QString::fromUtf8(file.readAll());
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
    const bool isLight = (currentTheme == Theme::Light);
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

// === Dashboard API ===
void MainWindow::setupDashboardPolling()
{
    if (!networkManager)
        networkManager = new QNetworkAccessManager(this);

    if (!supabaseClient) {
        supabaseClient = new SupabaseClient(this);
        supabaseClient->setBaseUrl(apiBaseUrl);
        connect(supabaseClient, &SupabaseClient::loginFinished, this, &MainWindow::handleAuthResult);
    }

    if (!dashboardTimer) {
        dashboardTimer = new QTimer(this);
        dashboardTimer->setInterval(dashboardRefreshIntervalMs);
        connect(dashboardTimer, &QTimer::timeout, this, &MainWindow::fetchDashboard);
        dashboardTimer->start();
    }

    refreshAuthToken();
    fetchDashboard();
}

void MainWindow::fetchDashboard()
{
    if (dashboardRequestInFlight || !networkManager)
        return;

    if (authToken.isEmpty()) {
        if (!authRefreshInFlight)
            refreshAuthToken();
        return;
    }

    QUrl url = apiBaseUrl.resolved(QUrl(QStringLiteral("/api/dashboard")));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());

    QNetworkReply* reply = networkManager->get(request);
    dashboardRequestInFlight = true;
    connect(reply, &QNetworkReply::finished, this, &MainWindow::handleDashboardReply);
}

void MainWindow::handleDashboardReply()
{
    dashboardRequestInFlight = false;
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "Dashboard request failed:" << reply->errorString() << "code" << statusCode;
        if (statusCode == 401 || statusCode == 403)
            refreshAuthToken();
        reply->deleteLater();
        return;
    }

    const QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    if (dashboardPage)
        dashboardPage->applyDashboardData(json);

    reply->deleteLater();
}

void MainWindow::refreshAuthToken()
{
    if (authRefreshInFlight)
        return;

    const QString envToken = qEnvironmentVariable("DASHBOARD_BEARER");
    if (!envToken.isEmpty()) {
        authToken = envToken;
        return;
    }

    if (!supabaseClient) {
        qWarning() << "Cannot refresh auth token: Supabase client not available";
        return;
    }

    const QString email = qEnvironmentVariable("APP_EMAIL");
    const QString password = qEnvironmentVariable("APP_PASSWORD");
    if (email.isEmpty() || password.isEmpty()) {
        qWarning() << "Auth token missing; set DASHBOARD_BEARER or APP_EMAIL/APP_PASSWORD to refresh automatically.";
        return;
    }

    authRefreshInFlight = true;
    supabaseClient->login(email, password);
}

void MainWindow::handleAuthResult(const AuthResult& result)
{
    authRefreshInFlight = false;
    if (!result.success) {
        qWarning() << "Auth refresh failed:" << result.message;
        return;
    }

    authToken = result.token;
    fetchDashboard();
}
