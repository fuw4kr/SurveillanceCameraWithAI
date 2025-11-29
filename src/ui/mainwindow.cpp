/**
 * @file MainWindow.cpp
 * @brief Implementation of the frameless primary application shell.
 *
 * This unit assembles the sidebar-driven workspace, wires AI processing
 * to UI pages, and manages theme loading, model selection, and periodic
 * dashboard polling. Networking is authenticated via Supabase, and model
 * paths are resolved from user settings or packaged defaults.
 *
 * @example
 * // Typical startup sequence after login:
 * MainWindow w;
 * w.initializeServerSync(session);
 * w.show();
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
#include <QDateTime>
#include <algorithm>

namespace {

QJsonObject extractSummaryObject(const QJsonDocument& doc)
{
    if (!doc.isObject())
        return {};

    const QJsonObject root = doc.object();
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    if (!data.isEmpty())
        return data;
    return root;
}

QList<int> parseHourlyDetections(const QJsonDocument& doc)
{
    QList<int> result(24, 0);
    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else {
        const QJsonObject obj = doc.object();
        arr = obj.value(QStringLiteral("detections")).toArray();
        if (arr.isEmpty())
            arr = obj.value(QStringLiteral("data")).toArray();
        if (arr.isEmpty())
            arr = obj.value(QStringLiteral("hours")).toArray();
        if (arr.isEmpty())
            arr = obj.value(QStringLiteral("items")).toArray();
    }

    if (arr.isEmpty())
        return result;

    if (arr.size() == result.size()) {
        bool allNumbers = true;
        for (const auto& v : arr) {
            if (!v.isDouble()) {
                allNumbers = false;
                break;
            }
        }
        if (allNumbers) {
            for (int i = 0; i < arr.size(); ++i)
                result[i] = arr.at(i).toInt();
            return result;
        }
    }

    bool anyData = false;
    int idx = 0;
    for (const auto& v : arr) {
        if (v.isDouble()) {
            if (idx < result.size())
                result[idx] = v.toInt();
            ++idx;
            anyData = true;
        } else if (v.isObject()) {
            const QJsonObject obj = v.toObject();
            const int hour = obj.value(QStringLiteral("hour")).toInt(
                obj.value(QStringLiteral("h")).toInt(obj.value(QStringLiteral("id")).toInt(-1)));
            const int count = obj.value(QStringLiteral("count")).toInt(
                obj.value(QStringLiteral("detections")).toInt(
                    obj.value(QStringLiteral("total")).toInt(obj.value(QStringLiteral("value")).toInt(0))));
            if (hour >= 0 && hour < result.size()) {
                result[hour] = count;
                anyData = true;
            }
        }
    }

    if (!anyData)
        result.fill(0);

    return result;
}

QString toTimeString(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();

    if (value.isDouble()) {
        const double raw = value.toDouble();
        const qint64 epochMs = raw > 4000000000.0 ? static_cast<qint64>(raw) : static_cast<qint64>(raw * 1000.0);
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(epochMs, Qt::UTC);
        if (dt.isValid())
            return dt.toLocalTime().toString(Qt::ISODate);
    }
    return {};
}

QJsonArray normalizeEvents(const QJsonDocument& doc)
{
    QJsonArray source;
    if (doc.isArray()) {
        source = doc.array();
    } else {
        const QJsonObject obj = doc.object();
        source = obj.value(QStringLiteral("events")).toArray();
        if (source.isEmpty())
            source = obj.value(QStringLiteral("data")).toArray();
        if (source.isEmpty())
            source = obj.value(QStringLiteral("items")).toArray();
    }

    QJsonArray normalized;
    for (const auto& entry : source) {
        const QJsonObject obj = entry.toObject();
        if (obj.isEmpty())
            continue;

        QString time = toTimeString(obj.value(QStringLiteral("time")));
        if (time.isEmpty())
            time = toTimeString(obj.value(QStringLiteral("timestamp")));
        if (time.isEmpty())
            time = toTimeString(obj.value(QStringLiteral("created_at")));
        if (time.isEmpty())
            time = toTimeString(obj.value(QStringLiteral("createdAt")));

        QString label = obj.value(QStringLiteral("label")).toString();
        if (label.isEmpty())
            label = obj.value(QStringLiteral("type")).toString();
        if (label.isEmpty())
            label = obj.value(QStringLiteral("event")).toString();
        if (label.isEmpty())
            label = obj.value(QStringLiteral("title")).toString();
        if (label.isEmpty())
            label = obj.value(QStringLiteral("category")).toString();
        if (label.isEmpty())
            label = obj.value(QStringLiteral("status")).toString();

        QString camera = obj.value(QStringLiteral("camera")).toString();
        if (camera.isEmpty())
            camera = obj.value(QStringLiteral("camera_name")).toString();
        if (camera.isEmpty())
            camera = obj.value(QStringLiteral("cameraName")).toString();
        if (camera.isEmpty())
            camera = obj.value(QStringLiteral("source")).toString();
        if (camera.isEmpty())
            camera = obj.value(QStringLiteral("device")).toString();
        if (camera.isEmpty()) {
            const QJsonValue camId = obj.contains(QStringLiteral("cameraId")) ? obj.value(QStringLiteral("cameraId")) : obj.value(QStringLiteral("camera_id"));
            if (camId.isDouble())
                camera = QString::number(camId.toInt());
            else
                camera = camId.toString();
        }

        QJsonObject normalizedEvent;
        if (!time.isEmpty())
            normalizedEvent.insert(QStringLiteral("time"), time);
        if (!label.isEmpty())
            normalizedEvent.insert(QStringLiteral("label"), label);
        if (!camera.isEmpty())
            normalizedEvent.insert(QStringLiteral("camera"), camera);

        if (!normalizedEvent.isEmpty())
            normalized.append(normalizedEvent);
    }

    return normalized;
}

} // namespace

/**
 * @brief Constructs the main window, assembles UI pages, and primes background services.
 *
 * Creates navigation, settings, and console areas; loads theme stylesheets; attempts to
 * resolve detection and embedding models from user settings or bundled defaults; and
 * moves the AI processor to a dedicated worker thread. Also instantiates the server
 * synchronization manager and face alert controller.
 *
 * @param parent Optional Qt parent for ownership and stacking.
 * @return MainWindow fully initialized but not yet shown.
 * @throws std::bad_alloc If widget or controller allocations fail.
 * @example
 * auto* window = new MainWindow();
 * window->initializeServerSync(session);
 * window->show();
 */
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

/**
 * @brief Shuts down worker threads and disposes auxiliary widgets.
 *
 * Ensures the AI thread quits and is deleted before releasing the processor pointer,
 * and hides/destroys the snapshot preview to avoid dangling visuals.
 *
 * @return void
 * @throws None No exceptions are thrown during teardown.
 * @example
 * delete window; // Safe cleanup of AI thread and preview window.
 */
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
/**
 * @brief Builds the central layout, navigation list, and stacked pages.
 *
 * Instantiates title bar placeholders, sidebar entries, stacked widgets for each page,
 * loads persisted model settings, allocates AI and camera managers, and connects early
 * page-level signals. Also bootstraps model loading with fallback candidates.
 *
 * @return void
 * @throws std::bad_alloc If any Qt widget allocation fails.
 * @example
 * setupUi(); // Called internally from the constructor.
 */
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
/**
 * @brief Configures the frameless title bar controls and window actions.
 *
 * Sets fixed height constraints for the title bar and wires minimize/close/maximize
 * buttons to the corresponding window slots.
 *
 * @return void
 * @throws None
 * @example
 * setupTitleBar(); // Invoked during construction.
 */
void MainWindow::setupTitleBar()
{
    titleBar->setMinimumHeight(36);
    titleBar->setMaximumHeight(36);

    connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
    connect(btnMinimize, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(btnMaximize, &QPushButton::clicked, this, &MainWindow::toggleMaximizeRestore);
}

/**
 * @brief Creates the settings popup for recognition cadence and GPU preference.
 *
 * Builds a popup widget with slider and checkbox controls that mirror cached settings,
 * and primes the UI with current values before showing.
 *
 * @return void
 * @throws None
 * @example
 * setupSettingsPopup();
 */
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
/**
 * @brief Initializes sidebar selection behavior and default selection.
 * @return void
 * @throws None
 * @example setupSidebar();
 */
void MainWindow::setupSidebar()
{
    listModes->setSelectionMode(QAbstractItemView::SingleSelection);
    listModes->setCurrentRow(0);
}

// === Console ===
/**
 * @brief Placeholder for console setup; reserved for future log view wiring.
 * @return void
 * @throws None
 * @example setupConsole();
 */
void MainWindow::setupConsole()
{
}

// === Connections ===
/**
 * @brief Connects UI controls to their handlers and wires AI processor signals.
 *
 * Binds sidebar navigation, settings toggles, model selection callbacks, and AI processor
 * change signals to update internal caches and UI state in response to runtime events.
 *
 * @return void
 * @throws None
 * @example setupConnections();
 */
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

/**
 * @brief Applies the requested theme by loading the corresponding stylesheet.
 *
 * Switches the global application stylesheet, updates the theme toggle text/tooltip,
 * and refreshes the maximize icon so glyph contrast matches the theme.
 *
 * @param theme Target theme enumeration value.
 * @return void
 * @throws None
 * @example applyTheme(Theme::Light);
 */
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

/**
 * @brief Loads both light and dark QSS stylesheets from resources.
 *
 * Reads bundled style files into memory for fast switching; logs warnings when
 * resources are missing or empty.
 *
 * @return void
 * @throws None
 * @example loadThemeStyles();
 */
void MainWindow::loadThemeStyles()
{
    lightStylesheet = loadStylesheet(":/resources/styles/light.qss");
    darkStylesheet = loadStylesheet(":/resources/styles/dark.qss");
    if (lightStylesheet.isEmpty() || darkStylesheet.isEmpty()) {
        qWarning() << "Theme stylesheets are missing or empty; theme switching may look incorrect.";
    }
}

/**
 * @brief Loads a stylesheet file from disk or Qt resources.
 *
 * Opens the given path as UTF-8 text and returns the content for application-wide
 * styling.
 *
 * @param path Qt resource URL or filesystem path to a .qss file.
 * @return QString Stylesheet contents, or empty on failure.
 * @throws None Errors are logged but not thrown.
 * @example QString css = loadStylesheet(":/resources/styles/dark.qss");
 */
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

/**
 * @brief Updates the window control glyph based on maximize state and theme.
 *
 * Chooses light or dark icon assets to ensure contrast remains legible in both
 * themes.
 *
 * @param maxed True when the window is maximized.
 * @return void
 * @throws None
 * @example updateMaximizeIcon(true);
 */
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

/**
 * @brief Toggles the settings popup visibility and repositions it relative to the settings button.
 *
 * Re-syncs UI controls with cached values before showing the popup and clamps its
 * position to the visible screen.
 *
 * @return void
 * @throws None
 * @example toggleSettingsPopup();
 */
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

/**
 * @brief Updates cached recognition interval and propagates it to the AI processor.
 *
 * Also updates the inline label to reflect the slider position for immediate
 * user feedback.
 *
 * @param value New interval in milliseconds.
 * @return void
 * @throws None
 * @example handleRecognitionSlider(750);
 */
void MainWindow::handleRecognitionSlider(int value)
{
    cachedRecognitionInterval = value;
    if (recognitionValueLabel)
        recognitionValueLabel->setText(QStringLiteral("%1 ms").arg(value));
    if (aiProcessor)
        QMetaObject::invokeMethod(aiProcessor, "setRecognitionIntervalMs", Qt::QueuedConnection, Q_ARG(int, value));
}

/**
 * @brief Toggles GPU preference for embedding inference and reloads the model if necessary.
 *
 * If an embedding model is already loaded, reloads it asynchronously to apply the
 * new backend preference.
 *
 * @param checked True to use GPU, false to prefer CPU.
 * @return void
 * @throws None
 * @example handleGpuToggle(false);
 */
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

/**
 * @brief Synchronizes UI controls with cached model and hardware preferences.
 *
 * Uses signal blockers to prevent feedback loops when updating slider and checkbox
 * states.
 *
 * @return void
 * @throws None
 * @example refreshSettingsUi();
 */
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
/**
 * @brief Configures dashboard networking, Supabase auth hooks, and polling timer.
 *
 * Ensures network manager and Supabase client exist, wires auth refresh callbacks,
 * initializes the periodic dashboard timer, and triggers an immediate fetch.
 *
 * @return void
 * @throws None
 * @example setupDashboardPolling();
 */
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

/**
 * @brief Issues an authenticated GET request to the dashboard API endpoint.
 *
 * Skips issuing if a request is already in flight or if an auth token is missing,
 * and triggers token refresh when required.
 *
 * @return void
 * @throws None
 * @example fetchDashboard();
 */
void MainWindow::fetchDashboard()
{
    if (dashboardRequestInFlight || !networkManager)
        return;

    if (authToken.isEmpty()) {
        if (!authRefreshInFlight)
            refreshAuthToken();
        return;
    }

    dashboardRequestInFlight = true;
    dashboardState.reset();
    pendingDashboardRequests = 0;

    const auto issueRequest = [this](const QString& path, const QString& key) {
        QUrl url = apiBaseUrl.resolved(QUrl(path));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        request.setRawHeader("Authorization", QByteArray("Bearer ") + authToken.toUtf8());

        QNetworkReply* reply = networkManager->get(request);
        reply->setProperty("dashboardKey", key);
        ++pendingDashboardRequests;
        connect(reply, &QNetworkReply::finished, this, &MainWindow::handleDashboardReply);
    };

    issueRequest(QStringLiteral("/api/stats/summary"), QStringLiteral("summary"));
    issueRequest(QStringLiteral("/api/stats/detections-by-hour"), QStringLiteral("detections_by_hour"));
    issueRequest(QStringLiteral("/api/stats/events"), QStringLiteral("events"));
}

/**
 * @brief Handles dashboard HTTP responses, updating UI or triggering auth refresh.
 *
 * Parses JSON payloads into the dashboard page and retries authentication on 401/403
 * responses.
 *
 * @return void
 * @throws None
 * @example handleDashboardReply();
 */
void MainWindow::handleDashboardReply()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    const QString key = reply->property("dashboardKey").toString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    auto finalize = [this]() {
        pendingDashboardRequests = std::max(0, pendingDashboardRequests - 1);
        if (pendingDashboardRequests == 0) {
            dashboardRequestInFlight = false;
            if (dashboardState.ready() && dashboardPage) {
                const QJsonObject payload = composeDashboardPayload();
                dashboardPage->applyDashboardData(payload);
                lastDashboardFetch = QDateTime::currentDateTime();
                qInfo() << "[Dashboard]" << "Refreshed"
                        << lastDashboardFetch.toString(Qt::ISODate)
                        << "cameras" << payload.value(QStringLiteral("cameras")).toInt()
                        << "detections" << payload.value(QStringLiteral("detections")).toInt()
                        << "alerts" << payload.value(QStringLiteral("alerts")).toInt();
            }
        }
    };

    if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
        qWarning() << "Dashboard request" << key << "failed:" << reply->errorString() << "code" << statusCode;
        if (statusCode == 401 || statusCode == 403) {
            refreshAuthToken();
        } else if (key == QStringLiteral("summary")) {
            dashboardState.summary = QJsonObject();
            dashboardState.hasSummary = true;
        } else if (key == QStringLiteral("detections_by_hour")) {
            dashboardState.hourlyDetections = QList<int>(24, 0);
            dashboardState.hasHourlyDetections = true;
        } else if (key == QStringLiteral("events")) {
            dashboardState.events = QJsonArray();
            dashboardState.hasEvents = true;
        }
        reply->deleteLater();
        finalize();
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "Dashboard response parse error for" << key << ":" << parseError.errorString();
        if (key == QStringLiteral("summary")) {
            dashboardState.summary = QJsonObject();
            dashboardState.hasSummary = true;
        } else if (key == QStringLiteral("detections_by_hour")) {
            dashboardState.hourlyDetections = QList<int>(24, 0);
            dashboardState.hasHourlyDetections = true;
        } else if (key == QStringLiteral("events")) {
            dashboardState.events = QJsonArray();
            dashboardState.hasEvents = true;
        }
        reply->deleteLater();
        finalize();
        return;
    }

    if (key == QStringLiteral("summary")) {
        dashboardState.summary = extractSummaryObject(doc);
        dashboardState.hasSummary = true;
    } else if (key == QStringLiteral("detections_by_hour")) {
        dashboardState.hourlyDetections = parseHourlyDetections(doc);
        dashboardState.hasHourlyDetections = true;
    } else if (key == QStringLiteral("events")) {
        dashboardState.events = normalizeEvents(doc);
        dashboardState.hasEvents = true;
    }

    reply->deleteLater();
    finalize();
}

QJsonObject MainWindow::composeDashboardPayload() const
{
    QJsonObject payload = dashboardState.summary;

    const auto pickInt = [](const QJsonObject& obj, const QStringList& keys, int fallback) {
        for (const QString& key : keys) {
            if (!obj.contains(key))
                continue;
            const QJsonValue v = obj.value(key);
            if (v.isDouble())
                return v.toInt();
            if (v.isString()) {
                bool ok = false;
                const int parsed = v.toString().toInt(&ok);
                if (ok)
                    return parsed;
            }
        }
        return fallback;
    };

    const auto pickBool = [](const QJsonObject& obj, const QStringList& keys, bool fallback) {
        for (const QString& key : keys) {
            if (!obj.contains(key))
                continue;
            const QJsonValue v = obj.value(key);
            if (v.isBool())
                return v.toBool();
            if (v.isDouble())
                return v.toInt() != 0;
            if (v.isString()) {
                const QString s = v.toString().trimmed().toLower();
                if (s == QStringLiteral("true") || s == QStringLiteral("yes") || s == QStringLiteral("1"))
                    return true;
                if (s == QStringLiteral("false") || s == QStringLiteral("no") || s == QStringLiteral("0"))
                    return false;
            }
        }
        return fallback;
    };

    payload.insert(QStringLiteral("cameras"), pickInt(payload, {
        QStringLiteral("cameras"),
        QStringLiteral("active_cameras"),
        QStringLiteral("activeCameras"),
        QStringLiteral("camerasOnline"),
        QStringLiteral("total_cameras")
    }, 0));

    payload.insert(QStringLiteral("detections"), pickInt(payload, {
        QStringLiteral("detections"),
        QStringLiteral("detections_today"),
        QStringLiteral("todayDetections"),
        QStringLiteral("totalDetections"),
        QStringLiteral("count")
    }, 0));

    payload.insert(QStringLiteral("alerts"), pickInt(payload, {
        QStringLiteral("alerts"),
        QStringLiteral("alerts_open"),
        QStringLiteral("openAlerts"),
        QStringLiteral("alertCount")
    }, 0));

    payload.insert(QStringLiteral("ai_active"), pickBool(payload, {
        QStringLiteral("ai_active"),
        QStringLiteral("aiActive"),
        QStringLiteral("ai_running"),
        QStringLiteral("aiRunning")
    }, false));

    QJsonArray activity;
    if (!dashboardState.hourlyDetections.isEmpty()) {
        for (int value : dashboardState.hourlyDetections)
            activity.append(value);
    }
    payload.insert(QStringLiteral("activity"), activity);
    payload.insert(QStringLiteral("events"), dashboardState.events);

    return payload;
}

/**
 * @brief Refreshes the authentication token using environment variables or Supabase.
 *
 * Prefers a pre-set bearer token; otherwise attempts email/password login. Guards
 * against concurrent refresh attempts.
 *
 * @return void
 * @throws None
 * @example refreshAuthToken();
 */
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

/**
 * @brief Consumes Supabase login results, updates stored token, and triggers dashboard fetch.
 *
 * Logs warnings on failures and resets the refresh-in-flight flag.
 *
 * @param result Authentication result from Supabase client.
 * @return void
 * @throws None
 * @example handleAuthResult(result);
 */
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

/**
 * @brief Applies a user-selected detection model path and persists preferences.
 *
 * Updates saved settings with relative paths and refreshes the settings page UI to
 * mirror the active detector and configuration.
 *
 * @param modelPath New detector model path chosen by the user.
 * @param configPath Optional detector config path.
 * @return void
 * @throws None
 * @example onDetectionModelSelected("C:/models/det.onnx", "C:/models/det.cfg");
 */
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

/**
 * @brief Persists the newly selected embedding model and reloads it through the AI processor.
 *
 * Converts the absolute path to a relative setting for portability, saves preferences,
 * and updates the settings page to reflect the active models.
 *
 * @param modelPath Filesystem path of the embedding model chosen by the user.
 * @return void
 * @throws None
 * @example onEmbeddingModelSelected("C:/models/arcface.onnx");
 */
void MainWindow::onEmbeddingModelSelected(const QString& modelPath)
{
    if (!loadEmbeddingModel(modelPath))
        return;
    modelSettings.embeddingModel = ModelSettings::toRelative(currentEmbeddingModel);
    modelSettings.save();
    if (settingsPage)
        settingsPage->setCurrentModels(currentDetectionModel, currentDetectionConfig, currentEmbeddingModel);
}

/**
 * @brief Attempts to load the user-selected detection model and persist the choice.
 *
 * Validates the provided paths, delegates loading to the AI processor, and updates
 * the settings page with the current model selections.
 *
 * @param modelPath Path to the detector model file.
 * @param configPath Optional configuration file path (e.g., Caffe prototxt).
 * @param warnOnFailure True to show a message box on failure.
 * @return bool True when the model loads successfully.
 * @throws None
 * @example loadDetectionModel("/models/scrfd.onnx", QString(), true);
 */
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

/**
 * @brief Loads a face embedding model and refreshes related UI state.
 *
 * Updates cached recognition interval and GPU preference from the AI processor
 * upon success, and refreshes settings widgets to reflect the active model.
 *
 * @param modelPath Path to the embedding model file.
 * @param warnOnFailure True to show a warning dialog on failure.
 * @return bool True if the model loads successfully.
 * @throws None
 * @example loadEmbeddingModel("/models/arcface.onnx");
 */
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

/**
 * @brief Updates the title-bar label with the active detection and embedding models.
 *
 * Displays just the file names to keep the bar compact, substituting "-" when a
 * model is missing.
 *
 * @return void
 * @throws None
 * @example updateModelInfoLabel();
 */
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

/**
 * @brief Logs informational server status messages.
 *
 * Intended for lightweight telemetry without surfacing UI notifications.
 *
 * @param status Status string emitted by the server sync manager.
 * @return void
 * @throws None
 * @example handleServerStatus("Refresh complete");
 */
void MainWindow::handleServerStatus(const QString& status)
{
    qInfo() << "[Server]" << status;
}

/**
 * @brief Logs server error messages for troubleshooting.
 *
 * @param error Error details emitted from server synchronization.
 * @return void
 * @throws None
 * @example handleServerError("Timeout while polling dashboard");
 */
void MainWindow::handleServerError(const QString& error)
{
    qWarning() << "[Server]" << error;
}

/**
 * @brief Applies login credentials to the sync manager and starts background sync.
 *
 * Transfers user email/password or bearer token from the login session and triggers
 * the synchronization loop, enabling cloud updates for pages such as the face database.
 *
 * @param session Result of the login dialog containing credentials and token.
 * @return void
 * @throws None
 * @example initializeServerSync(loginSession);
 */
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

/**
 * @brief Requests an immediate person refresh from the server sync manager.
 *
 * Provides users a manual override to fetch latest cloud data outside the timer
 * interval.
 *
 * @return void
 * @throws None
 * @example handleManualSync();
 */
void MainWindow::handleManualSync()
{
    if (!serverSync)
        return;
    qInfo() << "[MainWindow]" << "Manual sync requested by user";
    serverSync->requestImmediatePersonsRefresh();
}
