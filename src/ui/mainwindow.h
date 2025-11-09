/**
 * @file MainWindow.h
 * @brief Programmatic version of MainWindow (no .ui file)
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "windowEdit/FramelessWindow.h"
#include <QWidget>
#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QListView>
#include <QVBoxLayout>
#include <QHBoxLayout>

class MainWindow : public FramelessWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onModeChanged(int index);
    void updateMaximizeIcon(bool maxed);

private:
    // === UI elements ===
    QWidget* titleBar;
    QLabel* IconName;
    QLabel* titleLabel;
    QPushButton* btnMinimize;
    QPushButton* btnMaximize;
    QPushButton* btnClose;

    QListWidget* listModes;
    QStackedWidget* stackedWidget;
    QListView* consoleView;

    SnapPreviewWindow* snapPreview;

    // === Setup ===
    void setupUi();
    void setupTitleBar();
    void setupSidebar();
    void setupConsole();
    void setupConnections();
};

#endif // MAINWINDOW_H
