/**
 * @file MainWindow.cpp
 * @brief Implementation of MainWindow (no .ui file).
 */

#include "MainWindow.h"
#include "pages/dashboardpage.h"
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QStyle>

MainWindow::MainWindow(QWidget* parent)
    : FramelessWindow(parent)
{
    setupUi();
    setupTitleBar();
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
    if (snapPreview) {
        snapPreview->hidePreview();
        snapPreview->deleteLater();
    }
}

// === UI building ===
void MainWindow::setupUi()
{
    // ==== головний layout ====
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

    IconName = new QLabel("🧠");
    titleLabel = new QLabel("AI Smart Surveillance System");
    titleLabel->setStyleSheet("font-weight:600; color:#ccc;");
    btnMinimize = new QPushButton("–");
    btnMaximize = new QPushButton("▢");
    btnClose = new QPushButton("×");

    for (auto* b : { btnMinimize, btnMaximize, btnClose }) {
        b->setFixedSize(32, 28);
        b->setFlat(true);
    }

    titleLayout->addWidget(IconName);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(btnMinimize);
    titleLayout->addWidget(btnMaximize);
    titleLayout->addWidget(btnClose);

    // ==== центральна частина ====
    QHBoxLayout* centerLayout = new QHBoxLayout;
    centerLayout->setContentsMargins(4, 4, 4, 4);
    centerLayout->setSpacing(4);

    listModes = new QListWidget;
    listModes->addItems({
        "🏠 Dashboard",
        "🎥 Live Cameras",
        "🔥 Heatmap Analytics",
        "🧾 Events / Logs",
        "🧠 3D Face Viewer",
        "📈 AI Analytics",
        "💬 System Console",
        "⚙️ Settings"
        });
    listModes->setFixedWidth(220);

    stackedWidget = new QStackedWidget;
    stackedWidget->setObjectName("stackedWidget");
    auto dashboard = new DashboardPage;
    stackedWidget->addWidget(dashboard); // page 0 - Dashboard
    stackedWidget->addWidget(new QWidget); // page 1 - Cameras
    stackedWidget->addWidget(new QWidget); // page 2 - Heatmap
    stackedWidget->addWidget(new QWidget); // etc.
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);
    stackedWidget->addWidget(new QWidget);

    centerLayout->addWidget(listModes);
    centerLayout->addWidget(stackedWidget, 1);

    // ==== нижня консоль ====
    consoleView = new QListView;
    consoleView->setFixedHeight(180);

    // ==== розміщення ====
    mainLayout->addWidget(titleBar);
    mainLayout->addLayout(centerLayout, 1);
    mainLayout->addWidget(consoleView);

    setCentralWidget(central);
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
