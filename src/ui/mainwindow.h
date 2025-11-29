/**
 * @file MainWindow.h
 * @brief Programmatic definition of the primary application window.
 *
 * This header declares the frameless main shell that orchestrates all UI pages,
 * hardware integration, AI inference pipelines, and remote synchronization.
 * The window hosts navigation, status, and settings controls while coordinating
 * model loading and server-driven data refreshes.
 *
 * @example
 * // Create and display the main window after a successful login session.
 * MainWindow* window = new MainWindow();
 * window->initializeServerSync(session);
 * window->show();
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
    /**
     * @brief Builds the main UI chrome, loads models, and wires initial signals.
     *
     * The constructor configures the frameless window, allocates all child pages,
     * attempts to resolve default detection/embedding models, and primes the
     * dashboard polling timer. Heavy work such as AI processing is moved to a
     * dedicated thread.
     *
     * @param parent Optional Qt parent used for ownership and window parenting.
     * @throws std::bad_alloc If Qt widget allocations fail.
     * @example
     * MainWindow* w = new MainWindow(); // Parentless top-level window.
     */
    explicit MainWindow(QWidget* parent = nullptr);

    /**
     * @brief Tears down background threads and preview widgets gracefully.
     *
     * Ensures the AI worker thread is stopped and deleted, and disposes of the
     * snapshot preview window before the window is destroyed.
     *
     * @throws None No exceptions are expected; Qt cleanup is noexcept.
     * @example
     * delete w; // Destructor handles thread shutdown internally.
     */
    ~MainWindow();

    /**
     * @brief Starts periodic synchronization with the server using provided credentials.
     *
     * Applies email/password credentials or bearer token from the login session to
     * the server sync manager, then begins background polling and immediate person
     * refreshes.
     *
     * @param session Authentication context obtained from the login dialog.
     * @return void
     * @throws None No exceptions are thrown; failures are logged via Qt.
     * @example
     * MainWindow w;
     * w.initializeServerSync(loginResult);
     */
    void initializeServerSync(const LoginSession& session);

private slots:
    /**
     * @brief Switches the stacked widget to the selected mode.
     * @param index Row index from the sidebar list.
     * @return void
     * @throws None
     * @example onModeChanged(0); // Show dashboard page.
     */
    void onModeChanged(int index);
    /**
     * @brief Updates the maximize/restore icon based on window state.
     * @param maxed True when the window is maximized.
     * @return void
     * @throws None
     * @example updateMaximizeIcon(isMaximized());
     */
    void updateMaximizeIcon(bool maxed);
    /**
     * @brief Shows or hides the inline settings popup anchored to the title bar.
     * @return void
     * @throws None
     * @example toggleSettingsPopup();
     */
    void toggleSettingsPopup();
    /**
     * @brief Toggles between light and dark stylesheets.
     * @return void
     * @throws None
     * @example toggleTheme();
     */
    void toggleTheme();
    /**
     * @brief Applies and persists a new recognition interval from the settings slider.
     * @param value Interval in milliseconds.
     * @return void
     * @throws None
     * @example handleRecognitionSlider(500);
     */
    void handleRecognitionSlider(int value);
    /**
     * @brief Enables or disables GPU acceleration for embedding inference.
     * @param checked True to prefer GPU, false for CPU.
     * @return void
     * @throws None
     * @example handleGpuToggle(true);
     */
    void handleGpuToggle(bool checked);
    /**
     * @brief Processes HTTP responses from the dashboard API.
     * @return void
     * @throws None
     * @example handleDashboardReply();
     */
    void handleDashboardReply();
    /**
     * @brief Receives authentication refresh results from Supabase.
     * @param result Auth result containing token and expiry.
     * @return void
     * @throws None
     * @example handleAuthResult(authResult);
     */
    void handleAuthResult(const AuthResult& result);
    /**
     * @brief Loads the selected detection model from the settings page.
     * @param modelPath Filesystem path to the detector model.
     * @param configPath Optional configuration path for frameworks like Caffe.
     * @return void
     * @throws None
     * @example onDetectionModelSelected("/models/det.onnx", "/models/det.cfg");
     */
    void onDetectionModelSelected(const QString& modelPath, const QString& configPath);
    /**
     * @brief Loads the selected embedding model from the settings page.
     * @param modelPath Filesystem path to the embedding model.
     * @return void
     * @throws None
     * @example onEmbeddingModelSelected("/models/arcface.onnx");
     */
    void onEmbeddingModelSelected(const QString& modelPath);
    /**
     * @brief Updates status UI in response to server-side status messages.
     * @param status Human-readable status text.
     * @return void
     * @throws None
     * @example handleServerStatus("Sync complete");
     */
    void handleServerStatus(const QString& status);
    /**
     * @brief Logs server-side error messages for visibility.
     * @param error Error description emitted from server sync.
     * @return void
     * @throws None
     * @example handleServerError("Unauthorized");
     */
    void handleServerError(const QString& error);
    /**
     * @brief Forces an immediate synchronization cycle.
     * @return void
     * @throws None
     * @example handleManualSync();
     */
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
