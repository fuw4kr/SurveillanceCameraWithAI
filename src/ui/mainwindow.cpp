/**
 * @file MainWindow.cpp
 * @brief Implementation of MainWindow (no .ui file).
 */

#include "MainWindow.h"
#include "../core/ServerSyncManager.h"
#include "../core/DetectionEventController.h"
#include "FaceAlertController.h"
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
#include <QMessageBox>
#include <algorithm>

MainWindow::MainWindow(QWidget* parent)
    : FramelessWindow(parent)
{
    qInfo() << "[MainWindow]" << "Constructing UI";
    setupUi();
    setupTitleBar();
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
    titleLabel->setStyleSheet("font-weight:600; color:#ccc;");
    modelInfoLabel = new QLabel;
    modelInfoLabel->setObjectName("modelInfoLabel");
    modelInfoLabel->setStyleSheet("font-size:11px; color:#94a3b8;");
    modelInfoLabel->setAlignment(Qt::AlignVCenter);
    btnSync = new QPushButton(QStringLiteral("⟳"));
    titleLabel->setObjectName("titleLabel");
    btnThemeToggle = new QPushButton(tr("Dark"));
    btnThemeToggle->setObjectName("btnThemeToggle");
    btnSettings = new QPushButton(QStringLiteral("\u2699"));
    btnMinimize = new QPushButton("-");
    btnMaximize = new QPushButton("?");
    btnClose = new QPushButton("?");
    btnClose->setObjectName("btnClose");
    btnSettings->setObjectName("btnSettings");

    for (auto* b : { btnSync, btnSettings, btnMinimize, btnMaximize, btnClose }) {
        b->setFixedSize(32, 28);
        b->setFlat(true);
    }
    btnSync->setToolTip(tr("Sync now"));
    btnThemeToggle->setFixedHeight(28);
    btnThemeToggle->setMinimumWidth(72);
    btnThemeToggle->setFlat(true);
    btnSettings->setToolTip(tr("Settings"));

    titleLayout->addWidget(IconName);
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(modelInfoLabel, 0, Qt::AlignLeft);
    titleLayout->addStretch();
    titleLayout->addWidget(btnSync);
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
        "?? 3D Face Viewer",
        "?? AI Analytics",
        "?? Settings"
        });
    listModes->setFixedWidth(220);

    stackedWidget = new QStackedWidget;
    stackedWidget->setObjectName("stackedWidget");

    dashboardPage = new DashboardPage;
    modelsDirectory = QCoreApplication::applicationDirPath() + QStringLiteral("/assets/models");
    modelSettings = ModelSettings::load();

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
    const auto expandCandidates = [&](const QString& relative) {
        QString normalizedRel = relative;
        if (!normalizedRel.startsWith('/'))
            normalizedRel.prepend('/');
        QStringList expanded;
        const QStringList prefixes { QString(), QStringLiteral("/.."), QStringLiteral("/../.."), QStringLiteral("/../../..") };
        for (const QString& prefix : prefixes)
            expanded << binDir + prefix + normalizedRel;
        return expanded;
    };

    auto appendCandidates = [&](QStringList& target, const QString& relative) {
        target << expandCandidates(relative);
    };

    QStringList detectionCandidates;
    appendCandidates(detectionCandidates, "/assets/models/scrfd_2.5g_bnkps.onnx");
    appendCandidates(detectionCandidates, "/assets/models/face_detection_yunet_2023mar.onnx");
    appendCandidates(detectionCandidates, "/assets/models/buffalo_s/det_500m.onnx");
    appendCandidates(detectionCandidates, "/assets/models/det_500m.onnx");
    appendCandidates(detectionCandidates, "/assets/models/res10_300x300_ssd_iter_140000.caffemodel");
    const QString defaultDetectionModel = pickPath(detectionCandidates);
    const QString fallbackSsdModel = pickPath(expandCandidates("/assets/models/res10_300x300_ssd_iter_140000.caffemodel"));
    const QString fallbackSsdConfig = pickPath(expandCandidates("/assets/models/deploy.prototxt"));

    QStringList embeddingCandidates;
    appendCandidates(embeddingCandidates, "/assets/models/buffalo_s/w600k_mbf.onnx");
    appendCandidates(embeddingCandidates, "/assets/models/w600k_mbf.onnx");
    appendCandidates(embeddingCandidates, "/assets/models/arcface_r100.onnx");
    appendCandidates(embeddingCandidates, "/assets/models/arcface.onnx");
    appendCandidates(embeddingCandidates, "/assets/models/face_embedding.onnx");
    appendCandidates(embeddingCandidates, "/assets/models/mobilefacenet.onnx");
    const QString defaultEmbeddingModel = pickPath(embeddingCandidates);

    QString desiredDetectionModel = ModelSettings::resolvePath(modelSettings.detectionModel);
    QString desiredDetectionConfig = ModelSettings::resolvePath(modelSettings.detectionConfig);
    if (desiredDetectionModel.isEmpty())
        desiredDetectionModel = defaultDetectionModel;
    if (desiredDetectionConfig.isEmpty() && desiredDetectionModel.endsWith(QStringLiteral(".caffemodel"), Qt::CaseInsensitive))
        desiredDetectionConfig = fallbackSsdConfig;

    const auto tryDetection = [&](const QString& model, const QString& config) -> bool {
        if (model.isEmpty())
            return false;
        return loadDetectionModel(model, config, false);
    };

    if (!tryDetection(desiredDetectionModel, desiredDetectionConfig)) {
        if (defaultDetectionModel != desiredDetectionModel)
            tryDetection(defaultDetectionModel,
                defaultDetectionModel.endsWith(QStringLiteral(".caffemodel"), Qt::CaseInsensitive) ? fallbackSsdConfig : QString());
    }
    if (currentDetectionModel.isEmpty() && !fallbackSsdModel.isEmpty())
        loadDetectionModel(fallbackSsdModel, fallbackSsdConfig);

    QString desiredEmbeddingModel = ModelSettings::resolvePath(modelSettings.embeddingModel);
    if (desiredEmbeddingModel.isEmpty())
        desiredEmbeddingModel = defaultEmbeddingModel;

    const auto tryEmbedding = [&](const QString& model) -> bool {
        if (model.isEmpty())
            return false;
        return loadEmbeddingModel(model, false);
    };

    if (!tryEmbedding(desiredEmbeddingModel) && defaultEmbeddingModel != desiredEmbeddingModel)
        tryEmbedding(defaultEmbeddingModel);
    if (currentEmbeddingModel.isEmpty() && !defaultEmbeddingModel.isEmpty())
        loadEmbeddingModel(defaultEmbeddingModel);

    updateModelInfoLabel();
    cachedRecognitionInterval = aiProcessor ? aiProcessor->recognitionInterval() : cachedRecognitionInterval;
    cachedGpuPreference = aiProcessor ? aiProcessor->prefersGpuForEmbeddings() : cachedGpuPreference;

    aiThread = new QThread(this);
    aiProcessor->moveToThread(aiThread);
    connect(aiThread, &QThread::finished, aiProcessor, &QObject::deleteLater);
    aiThread->start();

    serverSync = new ServerSyncManager(this);
    serverSync->setAiProcessor(aiProcessor);
    serverSync->setCameraManager(cameraManager);

    camerasPage = new CamerasPage(cameraManager, aiProcessor, serverSync, this);
    faceDbPage = new FaceDatabasePage(serverSync, this);
    face3dPage = new Face3DViewerPage(aiProcessor, serverSync, this);
    analyticsPage = new AnalyticsPage(cameraManager, aiProcessor, this);

    settingsPage = new SettingsPage(modelsDirectory, this);
    connect(serverSync, &ServerSyncManager::personsUpdated, faceDbPage, &FaceDatabasePage::setRemotePersons);
    connect(serverSync, &ServerSyncManager::statusMessage, this, &MainWindow::handleServerStatus);
    connect(serverSync, &ServerSyncManager::errorMessage, this, &MainWindow::handleServerError);
    if (faceDbPage) {
        connect(faceDbPage, &FaceDatabasePage::requestCloudRefresh, this, [this]() {
            if (serverSync)
                serverSync->requestImmediatePersonsRefresh();
        });
    }
    faceAlertController = new FaceAlertController(aiProcessor, serverSync, this, this);
    detectionEventController = new DetectionEventController(aiProcessor, serverSync, this);

    stackedWidget->addWidget(dashboardPage);
    stackedWidget->addWidget(camerasPage);
    stackedWidget->addWidget(faceDbPage);
    stackedWidget->addWidget(face3dPage);
    stackedWidget->addWidget(analyticsPage);
    stackedWidget->addWidget(settingsPage);

    centerLayout->addWidget(listModes);
    centerLayout->addWidget(stackedWidget, 1);

    consoleView = new QListView;
    consoleView->setObjectName("consoleView");
    consoleView->setFixedHeight(180);

    mainLayout->addWidget(titleBar);
    mainLayout->addLayout(centerLayout, 1);

    setCentralWidget(central);

    connect(listModes, &QListWidget::currentRowChanged, this, [=](int index) {
        stackedWidget->setCurrentIndex(index);
    });

    if (settingsPage)
        settingsPage->setCurrentModels(currentDetectionModel, currentDetectionConfig, currentEmbeddingModel);

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
    if (btnSync)
        connect(btnSync, &QPushButton::clicked, this, &MainWindow::handleManualSync);
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

    if (settingsPage) {
        connect(settingsPage, &SettingsPage::detectionModelSelected,
            this, &MainWindow::onDetectionModelSelected);
        connect(settingsPage, &SettingsPage::embeddingModelSelected,
            this, &MainWindow::onEmbeddingModelSelected);
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
    if (!listModes || !stackedWidget || !settingsPage)
        return;
    const int settingsRow = listModes->count() - 1;
    if (settingsRow >= 0) {
        listModes->setCurrentRow(settingsRow);
        stackedWidget->setCurrentWidget(settingsPage);
    }
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
void MainWindow::onDetectionModelSelected(const QString& modelPath, const QString& configPath)
{
    if (!loadDetectionModel(modelPath, configPath))
        return;
    modelSettings.detectionModel = ModelSettings::toRelative(currentDetectionModel);
    modelSettings.detectionConfig = ModelSettings::toRelative(currentDetectionConfig);
    modelSettings.save();
    if (settingsPage)
        settingsPage->setCurrentModels(currentDetectionModel, currentDetectionConfig, currentEmbeddingModel);
}

void MainWindow::onEmbeddingModelSelected(const QString& modelPath)
{
    if (!loadEmbeddingModel(modelPath))
        return;
    modelSettings.embeddingModel = ModelSettings::toRelative(currentEmbeddingModel);
    modelSettings.save();
    if (settingsPage)
        settingsPage->setCurrentModels(currentDetectionModel, currentDetectionConfig, currentEmbeddingModel);
}

bool MainWindow::loadDetectionModel(const QString& modelPath, const QString& configPath, bool warnOnFailure)
{
    if (!aiProcessor || modelPath.isEmpty()) {
        if (warnOnFailure)
            QMessageBox::warning(this, tr("Face Detection"), tr("Please select a valid face detector model."));
        return false;
    }

    const QString resolvedModel = ModelSettings::resolvePath(modelPath);
    const QString resolvedConfig = ModelSettings::resolvePath(configPath);
    if (!aiProcessor->loadFaceModel(resolvedModel, resolvedConfig)) {
        if (warnOnFailure) {
            QMessageBox::warning(this, tr("Face Detection"),
                tr("Failed to load face detector:\n%1").arg(resolvedModel));
        }
        return false;
    }

    currentDetectionModel = resolvedModel;
    currentDetectionConfig = resolvedConfig;
    qInfo() << "Face detector loaded:" << resolvedModel;
    updateModelInfoLabel();
    return true;
}

bool MainWindow::loadEmbeddingModel(const QString& modelPath, bool warnOnFailure)
{
    if (!aiProcessor || modelPath.isEmpty()) {
        if (warnOnFailure)
            QMessageBox::warning(this, tr("Face Recognition"), tr("Please select a valid embedding model."));
        return false;
    }

    const QString resolvedModel = ModelSettings::resolvePath(modelPath);
    if (!aiProcessor->loadEmbedModel(resolvedModel)) {
        if (warnOnFailure) {
            QMessageBox::warning(this, tr("Face Recognition"),
                tr("Failed to load embedding model:\n%1").arg(resolvedModel));
        }
        return false;
    }

    embedModelPath = resolvedModel;
    currentEmbeddingModel = resolvedModel;
    cachedRecognitionInterval = aiProcessor->recognitionInterval();
    cachedGpuPreference = aiProcessor->prefersGpuForEmbeddings();
    refreshSettingsUi();
    qInfo() << "Embedding model loaded:" << resolvedModel;
    updateModelInfoLabel();
    return true;
}

void MainWindow::updateModelInfoLabel()
{
    if (!modelInfoLabel)
        return;

    auto toLabel = [](const QString& path) -> QString {
        if (path.isEmpty())
            return QStringLiteral("-");
        return QFileInfo(path).fileName();
    };

    modelInfoLabel->setText(
        tr("Det: %1 | Emb: %2")
            .arg(toLabel(currentDetectionModel),
                toLabel(currentEmbeddingModel)));
}

void MainWindow::handleServerStatus(const QString& status)
{
    qInfo() << "[Server]" << status;
}

void MainWindow::handleServerError(const QString& error)
{
    qWarning() << "[Server]" << error;
}

void MainWindow::initializeServerSync(const LoginSession& session)
{
    if (!serverSync)
        return;
    if (!session.email.isEmpty() || !session.password.isEmpty())
        serverSync->setCredentials(session.email, session.password);
    if (session.auth.success && !session.auth.token.isEmpty())
        serverSync->applySessionToken(session.auth.token, session.auth.expiresAt);
    serverSync->start();
    qInfo() << "[MainWindow]" << "Server synchronization initialized";
}

void MainWindow::handleManualSync()
{
    if (!serverSync)
        return;
    qInfo() << "[MainWindow]" << "Manual sync requested by user";
    serverSync->requestImmediatePersonsRefresh();
}
