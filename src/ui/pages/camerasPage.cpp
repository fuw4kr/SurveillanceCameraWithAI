#include "CamerasPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QDebug>
#include <QMutexLocker>
#include <QSize>

CamerasPage::CamerasPage(CameraManager* manager, AIProcessor* processor, QWidget* parent)
    : QWidget(parent)
    , cameraManager(manager)
    , aiProcessor(processor)
{
    setupUi();
    connect(cameraManager, &CameraManager::frameReady, this, &CamerasPage::onFrameReady, Qt::QueuedConnection);
    if (aiProcessor) {
        connect(this, &CamerasPage::requestProcessFrame, aiProcessor, &AIProcessor::processFrameAsync, Qt::QueuedConnection);
        connect(aiProcessor, &AIProcessor::frameProcessed, this, &CamerasPage::onFrameProcessed, Qt::QueuedConnection);
    }
}

void CamerasPage::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    auto* controls = new QHBoxLayout;
    controls->setSpacing(8);

    auto* localLabel = new QLabel(tr("Local index:"));
    indexSpin = new QSpinBox;
    indexSpin->setRange(0, 9);
    btnAddIndex = new QPushButton(tr("Add Local"));

    rtspInput = new QLineEdit;
    rtspInput->setPlaceholderText(tr("rtsp://, http:// or file path"));
    btnAddRtsp = new QPushButton(tr("Add RTSP/URL"));

    btnPrev = new QPushButton(tr("◀"));
    btnNext = new QPushButton(tr("▶"));

    controls->addWidget(localLabel);
    controls->addWidget(indexSpin);
    controls->addWidget(btnAddIndex);
    controls->addSpacing(12);
    controls->addWidget(rtspInput, 1);
    controls->addWidget(btnAddRtsp);
    controls->addStretch();
    controls->addWidget(btnPrev);
    controls->addWidget(btnNext);

    connect(btnAddIndex, &QPushButton::clicked, this, &CamerasPage::onAddLocalCamera);
    connect(btnAddRtsp, &QPushButton::clicked, this, &CamerasPage::onAddRtspCamera);
    connect(btnPrev, &QPushButton::clicked, this, &CamerasPage::onPrevPage);
    connect(btnNext, &QPushButton::clicked, this, &CamerasPage::onNextPage);

    gridContainer = new QWidget;
    grid = new QGridLayout(gridContainer);
    grid->setSpacing(12);
    grid->setContentsMargins(12, 12, 12, 12);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(gridContainer);
    scroll->setStyleSheet("background:#0d1117; border:none;");

    mainLayout->addLayout(controls);
    mainLayout->addWidget(scroll, 1);
}

void CamerasPage::addCameraSource(const QString& source, const QString& title)
{
    if (source.isEmpty() || !cameraManager)
        return;

    const int id = nextCameraId++;
    if (!cameraManager->openCamera(id, source)) {
        qWarning() << "Failed to open camera source" << source;
        return;
    }

    auto* view = new CameraViewWidget(id, this);
    view->setTitle(title.isEmpty() ? tr("Camera %1").arg(id) : title);
    connect(view, &CameraViewWidget::toggleRequested, this, &CamerasPage::onToggleCamera);
    connect(view, &CameraViewWidget::audioToggled, this, [this](int camId, bool enabled) {
        if (!cameraManager)
            return;
        if (enabled)
            cameraManager->enableAudio(camId);
        else
            cameraManager->disableAudio(camId);
    });
    connect(view, &CameraViewWidget::removeRequested, this, &CamerasPage::onRemoveCamera);
    cameraWidgets.insert(id, view);
    cameraOrder.append(id);
    {
        QMutexLocker locker(&detectionMutex);
        cameraStates.remove(id);
    }
    view->setCameraActive(true);
    view->setAudioVisible(true);

    cameraManager->startCapture(id);
    updateGrid();
}

void CamerasPage::onAddLocalCamera()
{
    addCameraSource(QString::number(indexSpin->value()), tr("Local #%1").arg(indexSpin->value()));
}

void CamerasPage::onAddRtspCamera()
{
    const QString src = rtspInput->text().trimmed();
    if (src.isEmpty())
        return;
    addCameraSource(src, src);
    rtspInput->clear();
}

void CamerasPage::updateGrid()
{
    QLayoutItem* item = nullptr;
    while ((item = grid->takeAt(0)) != nullptr) {
        if (auto* w = item->widget())
            w->setParent(nullptr);
        delete item;
    }

    const int start = currentPage * camsPerPage;
    const int end = qMin(start + camsPerPage, cameraOrder.size());

    int row = 0;
    int col = 0;
    for (int i = start; i < end; ++i) {
        int cameraId = cameraOrder[i];
        auto* view = cameraWidgets.value(cameraId, nullptr);
        if (!view)
            continue;
        grid->addWidget(view, row, col);
        if (++col == 3) {
            col = 0;
            ++row;
        }
    }
}

void CamerasPage::onFrameReady(int id, const QImage& frame)
{
    auto* view = cameraWidgets.value(id, nullptr);
    if (!view || frame.isNull())
        return;

    bool shouldProcess = false;
    bool allowRawDisplay = !aiProcessor;
    {
        QMutexLocker locker(&detectionMutex);
        auto& state = cameraStates[id];
        allowRawDisplay = allowRawDisplay || !state.hasAnnotated;
        if (aiProcessor && !state.pending && !aiBusy.load()) {
            state.pending = true;
            shouldProcess = true;
            aiBusy.store(true);
        }
    }

    if (allowRawDisplay)
        view->updateFrame(frame);

    if (shouldProcess)
        emit requestProcessFrame(id, frame);
}

void CamerasPage::onFrameProcessed(int id, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize)
{
    Q_UNUSED(detections);
    Q_UNUSED(sourceSize);

    {
        QMutexLocker locker(&detectionMutex);
        auto& state = cameraStates[id];
        state.pending = false;
        state.hasAnnotated = true;
    }
    aiBusy.store(false);

    auto* view = cameraWidgets.value(id, nullptr);
    if (view && !annotated.isNull())
        view->updateFrame(annotated);
}

void CamerasPage::onToggleCamera(int id, bool enable)
{
    if (!cameraManager)
        return;
    if (enable)
        cameraManager->startCapture(id);
    else
        cameraManager->stopCapture(id);
    if (auto* view = cameraWidgets.value(id, nullptr))
        view->setCameraActive(enable);
}

void CamerasPage::onRemoveCamera(int id)
{
    if (!cameraManager || !cameraWidgets.contains(id))
        return;

    cameraManager->stopCapture(id);
    cameraManager->closeCamera(id);

    if (auto* view = cameraWidgets.take(id)) {
        cameraOrder.removeAll(id);
        QMutexLocker locker(&detectionMutex);
        cameraStates.remove(id);
        view->deleteLater();
    }
    if (currentPage * camsPerPage >= cameraOrder.size() && currentPage > 0)
        --currentPage;
    updateGrid();
}

void CamerasPage::onNextPage()
{
    if ((currentPage + 1) * camsPerPage < cameraOrder.size()) {
        ++currentPage;
        updateGrid();
    }
}

void CamerasPage::onPrevPage()
{
    if (currentPage > 0) {
        --currentPage;
        updateGrid();
    }
}
