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
#include "pages/face3dviewerpage.h"
#include "pages/faceDatabasePage.h"
#include "pages/settingsPage.h"
#include "../core/CameraManager.h"
#include "../core/AIProcessor.h"
#include "../core/modelSettings.h"
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

class ServerSyncManager;

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
    void handleRecognitionSlider(int value);
    void handleGpuToggle(bool checked);
    void onDetectionModelSelected(const QString& modelPath, const QString& configPath);
    void onEmbeddingModelSelected(const QString& modelPath);
    void handleServerStatus(const QString& status);
    void handleServerError(const QString& error);

private:
    // === UI elements ===
    QWidget* titleBar;
    QLabel* IconName;
    QLabel* titleLabel;
    QLabel* modelInfoLabel = nullptr;
    QPushButton* btnMinimize;
    QPushButton* btnMaximize;
    QPushButton* btnClose;
    QPushButton* btnSettings;

    QListWidget* listModes;
    QStackedWidget* stackedWidget;
    QListView* consoleView;

    SnapPreviewWindow* snapPreview;

    QWidget* settingsPopup = nullptr;
    QSlider* recognitionSlider = nullptr;
    QLabel* recognitionValueLabel = nullptr;
    QCheckBox* gpuToggle = nullptr;

    CameraManager* cameraManager = nullptr;
    AIProcessor* aiProcessor = nullptr;
    CamerasPage* camerasPage = nullptr;
    AnalyticsPage* analyticsPage = nullptr;
    FaceDatabasePage* faceDbPage = nullptr;
    Face3DViewerPage* face3dPage = nullptr;
    SettingsPage* settingsPage = nullptr;
    ServerSyncManager* serverSync = nullptr;
    QString embedModelPath;
    QThread* aiThread = nullptr;
    int cachedRecognitionInterval = 500;
    bool cachedGpuPreference = true;
    ModelSettings modelSettings;
    QString currentDetectionModel;
    QString currentDetectionConfig;
    QString currentEmbeddingModel;
    QString modelsDirectory;

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
};

#endif // MAINWINDOW_H
