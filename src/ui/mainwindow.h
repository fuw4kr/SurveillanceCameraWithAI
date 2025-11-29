/**
 * @file MainWindow.h
 * @brief Programmatic version of MainWindow (no .ui file)
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "windowEdit/FramelessWindow.h"
#include "pages/dashboardpage.h"
#include "pages/CamerasPage.h"
#include "pages/analyticsPage.h"
#include "pages/faceDatabasePage.h"
#include "../core/CameraManager.h"
#include "../core/AIProcessor.h"
#include "../core/SupabaseClient.h"
#include <QWidget>
#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QListView>
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

class MainWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onModeChanged(int index);
    void updateMaximizeIcon(bool maxed);
    void toggleSettingsPopup();
    void toggleTheme();
    void handleRecognitionSlider(int value);
    void handleGpuToggle(bool checked);
    void handleDashboardReply();
    void handleAuthResult(const AuthResult& result);

private:
    enum class Theme {
        Light,
        Dark
    };

    // === UI elements ===
    QWidget* titleBar;
    QLabel* IconName;
    QLabel* titleLabel;
    QPushButton* btnThemeToggle;
    QPushButton* btnMinimize;
    QPushButton* btnMaximize;
    QPushButton* btnClose;
    QPushButton* btnSettings;

    QListWidget* listModes;
    QStackedWidget* stackedWidget;
    QListView* consoleView;

    SnapPreviewWindow* snapPreview;
    DashboardPage* dashboardPage = nullptr;

    QWidget* settingsPopup = nullptr;
    QSlider* recognitionSlider = nullptr;
    QLabel* recognitionValueLabel = nullptr;
    QCheckBox* gpuToggle = nullptr;

    CameraManager* cameraManager = nullptr;
    AIProcessor* aiProcessor = nullptr;
    CamerasPage* camerasPage = nullptr;
    AnalyticsPage* analyticsPage = nullptr;
    FaceDatabasePage* faceDbPage = nullptr;
    QString embedModelPath;
    QThread* aiThread = nullptr;
    int cachedRecognitionInterval = 500;
    bool cachedGpuPreference = true;
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
    void applyTheme(Theme theme);
    void loadThemeStyles();
    QString loadStylesheet(const QString& path) const;
    void setupDashboardPolling();
    void fetchDashboard();
    void refreshAuthToken();
};

#endif // MAINWINDOW_H
