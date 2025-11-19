#include "CamerasPage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>

CamerasPage::CamerasPage(CameraManager* manager, QWidget* parent)
    : QWidget(parent), cameraManager(manager)
{
    setupUi();
    connect(cameraManager, &CameraManager::frameReady, this, &CamerasPage::onFrameReady);
}

void CamerasPage::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    // === Панель управління ===
    auto* controls = new QHBoxLayout;
    ipInput = new QLineEdit;
    ipInput->setPlaceholderText("http://192.168.0.100:8080");
    btnAddCamera = new QPushButton("➕ Add Camera");
    btnPrev = new QPushButton("◀");
    btnNext = new QPushButton("▶");

    controls->addWidget(ipInput, 1);
    controls->addWidget(btnAddCamera);
    controls->addStretch();
    controls->addWidget(btnPrev);
    controls->addWidget(btnNext);

    connect(btnAddCamera, &QPushButton::clicked, this, &CamerasPage::onAddCameraClicked);
    connect(btnPrev, &QPushButton::clicked, this, &CamerasPage::onPrevPage);
    connect(btnNext, &QPushButton::clicked, this, &CamerasPage::onNextPage);

    // === Сітка камер ===
    gridContainer = new QWidget;
    grid = new QGridLayout(gridContainer);
    grid->setSpacing(8);
    grid->setContentsMargins(8, 8, 8, 8);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(gridContainer);
    scroll->setStyleSheet("background:#121212; border:none;");

    mainLayout->addLayout(controls);
    mainLayout->addWidget(scroll, 1);
}

void CamerasPage::onAddCameraClicked()
{
    QString src = ipInput->text().trimmed();
    if (src.isEmpty()) return;

    int id = cameraWidgets.size();
    if (cameraManager->openCamera(id, src)) {
        auto* view = new CameraViewWidget(id, src);
        connect(view, &CameraViewWidget::toggleRequested, this, &CamerasPage::onToggleCamera);
        connect(view, &CameraViewWidget::audioToggled, this, [=](int id, bool on) {
            if (on)
                cameraManager->enableAudio(id);
            else
                cameraManager->disableAudio(id);
            });
        cameraWidgets << view;
        cameraManager->startCapture(id);
        updateGrid();
    }
}


void CamerasPage::updateGrid()
{
    QLayoutItem* item;
    while ((item = grid->takeAt(0))) {
        if (auto w = item->widget()) w->setParent(nullptr);
        delete item;
    }

    int start = currentPage * camsPerPage;
    int end = std::min(start + camsPerPage, (int)cameraWidgets.size());

    int row = 0, col = 0;
    for (int i = start; i < end; ++i) {
        grid->addWidget(cameraWidgets[i], row, col);
        if (++col == 3) { col = 0; ++row; }
    }
}

void CamerasPage::onFrameReady(int id, const QImage& frame)
{
    if (id >= 0 && id < cameraWidgets.size())
        cameraWidgets[id]->updateFrame(frame);
}

void CamerasPage::onToggleCamera(int id, bool enable)
{
    if (enable)
        cameraManager->startCapture(id);
    else
        cameraManager->stopCapture(id);
}

void CamerasPage::onNextPage()
{
    if ((currentPage + 1) * camsPerPage < cameraWidgets.size()) {
        currentPage++;
        updateGrid();
    }
}

void CamerasPage::onPrevPage()
{
    if (currentPage > 0) {
        currentPage--;
        updateGrid();
    }
}
