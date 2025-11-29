/**
 * @file MainWindow.h
 * @brief Programmatic version of MainWindow (no .ui file)
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "windowEdit/FramelessWindow.h"
#include "pages/dashboardpage.h"
#include "pages/CamerasPage.h"
#include "pages/face3dviewerpage.h"
#include "pages/faceDatabasePage.h"
#include "pages/settingsPage.h"
#include "pages/analyticsPage.h"
#include "../core/CameraManager.h"
#include "../core/AIProcessor.h"
#include "../core/modelSettings.h"
#include "../core/SupabaseClient.h"
#include <QWidget>
#include <QMainWindow>
#include <QListWidget>
#include <QListView>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QCheckBox>
#include <QString>
#include <QThread>
#include <QMetaObject>
#include <QUrl>

class QNetworkAccessManager;
class QTimer;

class ServerSyncManager;
class FaceAlertController;
class DetectionEventController;

class MainWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    void initializeServerSync(const LoginSession& session);

private slots:
    void onModeChanged(int index);
    void updateMaximizeIcon(bool maxed);
    void toggleSettingsPopup();
    void toggleTheme();
    void handleRecognitionSlider(int value);
    void handleGpuToggle(bool checked);
    void handleDashboardReply();
    void handleAuthResult(const AuthResult& result);
    void onDetectionModelSelected(const QString& modelPath, const QString& configPath);
    void onEmbeddingModelSelected(const QString& modelPath);
    void handleServerStatus(const QString& status);
    void handleServerError(const QString& error);
    void handleManualSync();

private:
    enum class Theme {
        Light,
        Dark
    };

    // === UI elements ===
    QWidget* titleBar;
    QLabel* IconName;
    QLabel* titleLabel;
    QLabel* modelInfoLabel = nullptr;
    QPushButton* btnThemeToggle;
    QPushButton* btnMinimize;
    QPushButton* btnSync;
    QPushButton* btnMaximize;
    QPushButton* btnClose;
    QPushButton* btnSettings;

    QListWidget* listModes;
    QStackedWidget* stackedWidget;
    SnapPreviewWindow* snapPreview;
    QListView* consoleView = nullptr;
    DashboardPage* dashboardPage = nullptr;

    QWidget* settingsPopup = nullptr;
    QSlider* recognitionSlider = nullptr;
    QLabel* recognitionValueLabel = nullptr;
    QCheckBox* gpuToggle = nullptr;

    CameraManager* cameraManager = nullptr;
    AIProcessor* aiProcessor = nullptr;
    CamerasPage* camerasPage = nullptr;
    FaceDatabasePage* faceDbPage = nullptr;
    Face3DViewerPage* face3dPage = nullptr;
    AnalyticsPage* analyticsPage = nullptr;
    SettingsPage* settingsPage = nullptr;
    ServerSyncManager* serverSync = nullptr;
    FaceAlertController* faceAlertController = nullptr;
    DetectionEventController* detectionEventController = nullptr;
    QString embedModelPath;
    QThread* aiThread = nullptr;
    int cachedRecognitionInterval = 500;
    bool cachedGpuPreference = true;
    ModelSettings modelSettings;
    QString currentDetectionModel;
    QString currentDetectionConfig;
    QString currentEmbeddingModel;
    QString modelsDirectory;
    Theme currentTheme = Theme::Dark;
    QString darkStylesheet;
    QString lightStylesheet;
    QNetworkAccessManager* networkManager = nullptr;
    QTimer* dashboardTimer = nullptr;
    SupabaseClient* supabaseClient = nullptr;
    QString authToken;
    bool dashboardRequestInFlight = false;
    bool authRefreshInFlight = false;
    QUrl apiBaseUrl{ QStringLiteral("https://myserver-tc2d.onrender.com") };
    int dashboardRefreshIntervalMs = 60000;

    // === Setup ===
    void setupUi();
    void setupTitleBar();
    void setupSettingsPopup();
    void refreshSettingsUi();
    void setupSidebar();
    void setupConsole();
    void setupConnections();
    bool loadDetectionModel(const QString& modelPath, const QString& configPath, bool warnOnFailure = true);
    bool loadEmbeddingModel(const QString& modelPath, bool warnOnFailure = true);
    void updateModelInfoLabel();
    void applyTheme(Theme theme);
    void loadThemeStyles();
    QString loadStylesheet(const QString& path) const;
    void setupDashboardPolling();
    void fetchDashboard();
    void refreshAuthToken();
};

#endif // MAINWINDOW_H
