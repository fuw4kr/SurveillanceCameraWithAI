#include "CamerasPage.h"

#include "../../core/ServerSyncManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QDebug>
#include <QMutexLocker>
#include <QSize>
#include <QListWidget>
#include <QAbstractItemView>
#include <QMenu>
#include <QCursor>

CamerasPage::CamerasPage(CameraManager* manager, AIProcessor* processor, ServerSyncManager* sync, QWidget* parent)
    : QWidget(parent)
    , cameraManager(manager)
    , serverSync(sync)
    , aiProcessor(processor)
{
    setupUi();
    connect(cameraManager, &CameraManager::frameReady, this, &CamerasPage::onFrameReady, Qt::QueuedConnection);
    if (aiProcessor) {
        connect(this, &CamerasPage::requestProcessFrame, aiProcessor, &AIProcessor::processFrameAsync, Qt::QueuedConnection);
        connect(aiProcessor, &AIProcessor::frameProcessed, this, &CamerasPage::onFrameProcessed, Qt::QueuedConnection);
    }
    if (serverSync) {
        connect(serverSync, &ServerSyncManager::camerasUpdated,
            this, &CamerasPage::handleRemoteCamerasUpdated);
        serverSync->requestImmediateCamerasRefresh();
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

    QLabel* cloudLabel = new QLabel(tr("Cloud cameras"), this);
    cloudLabel->setStyleSheet(QStringLiteral("font-weight:600; color:#94a3b8;"));
    mainLayout->addWidget(cloudLabel);

    remoteCameraList = new QListWidget(this);
    remoteCameraList->setObjectName(QStringLiteral("remoteCameraList"));
    remoteCameraList->setSelectionMode(QAbstractItemView::NoSelection);
    remoteCameraList->setStyleSheet(QStringLiteral("QListWidget { background:#0d1117; border:1px solid #1f2937; border-radius:8px; } QListWidget::item { color:#e2e8f0; }"));
    remoteCameraList->setMinimumHeight(150);
    remoteCameraList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(remoteCameraList, &QListWidget::itemDoubleClicked, this, &CamerasPage::handleRemoteCameraDoubleClick);
    connect(remoteCameraList, &QListWidget::customContextMenuRequested, this, &CamerasPage::handleRemoteListContextMenu);
    mainLayout->addWidget(remoteCameraList);
}

void CamerasPage::addCameraSource(const QString& source, const QString& title, bool registerOnServer)
{
    if (source.isEmpty() || !cameraManager)
        return;

    const int id = nextCameraId++;
    if (!cameraManager->openCamera(id, source)) {
        qWarning() << "Failed to open camera source" << source;
        return;
    }

    auto* view = new CameraViewWidget(id, this);
    const QString label = title.isEmpty() ? tr("Camera %1").arg(id) : title;
    view->setTitle(label);
    cameraSources.insert(id, source);
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

    if (registerOnServer)
        publishCameraToServer(label, source);
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

    if (serverSync) {
        const QString source = cameraSources.value(id);
        const QString cameraId = remoteCameraIdForStream(source);
        if (!cameraId.isEmpty())
            serverSync->updateCameraStatus(cameraId, enable ? QStringLiteral("active") : QStringLiteral("offline"));
    }
}

void CamerasPage::onRemoveCamera(int id)
{
    if (!cameraManager || !cameraWidgets.contains(id))
        return;

    cameraManager->stopCapture(id);
    cameraManager->closeCamera(id);

    if (auto* view = cameraWidgets.take(id)) {
        cameraOrder.removeAll(id);
        cameraSources.remove(id);
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

void CamerasPage::handleRemoteCamerasUpdated(const QList<CameraRecord>& cameras)
{
    knownRemoteCameras = cameras;
    updateRemoteListView();
}

void CamerasPage::updateRemoteListView()
{
    if (!remoteCameraList)
        return;
    remoteCameraList->clear();
    QSet<QString> seenStreams;
    for (const auto& camera : knownRemoteCameras) {
        const QString stream = camera.streamUrl.trimmed();
        const QString normalized = stream.toLower();
        const QString name = camera.name.isEmpty() ? stream : camera.name;
        const QString status = camera.status.isEmpty() ? tr("Unknown") : camera.status;
        QString summary = QStringLiteral("%1 - %2").arg(name, status);
        if (!stream.isEmpty() && seenStreams.contains(normalized))
            summary.append(tr(" (duplicate)"));
        QString subtitle = stream;
        if (!camera.location.isEmpty())
            subtitle += QStringLiteral(" (%1)").arg(camera.location);
        auto* item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(summary, subtitle), remoteCameraList);
        item->setToolTip(stream.isEmpty() ? camera.id : stream);
        item->setData(Qt::UserRole, camera.id);
        item->setData(Qt::UserRole + 1, stream);
        item->setData(Qt::UserRole + 2, name);
        seenStreams.insert(normalized);
    }
}

void CamerasPage::publishCameraToServer(const QString& name, const QString& streamUrl)
{
    if (!serverSync)
        return;
    if (cameraExistsOnServer(streamUrl)) {
        qInfo() << "[CamerasPage]" << "Camera already registered remotely, skipping duplicate:" << streamUrl;
        return;
    }
    const QString label = name.isEmpty() ? streamUrl : name;
    QString ip;
    if (streamUrl.startsWith(QStringLiteral("local://")) || streamUrl.startsWith(QStringLiteral("local")))
        ip = streamUrl;
    serverSync->submitCameraRecord(label, streamUrl, ip);
}

void CamerasPage::openRemoteCamera(const CameraRecord& record)
{
    if (record.streamUrl.isEmpty())
        return;
    const QString label = record.name.isEmpty() ? record.streamUrl : record.name;
    addCameraSource(record.streamUrl, label, false);
}

void CamerasPage::handleRemoteCameraDoubleClick(QListWidgetItem* item)
{
    if (!item)
        return;
    const QString cameraId = item->data(Qt::UserRole).toString();
    CameraRecord record = remoteCameraById(cameraId);
    if (record.id.isEmpty()) {
        record.id = cameraId;
        record.streamUrl = item->data(Qt::UserRole + 1).toString();
        record.name = item->data(Qt::UserRole + 2).toString();
    }
    openRemoteCamera(record);
}

void CamerasPage::handleRemoteListContextMenu(const QPoint& pos)
{
    if (!remoteCameraList)
        return;
    QListWidgetItem* item = remoteCameraList->itemAt(pos);
    QMenu menu(this);
    QAction* openAction = menu.addAction(tr("Open locally"));
    QAction* deleteAction = menu.addAction(tr("Delete from server"));
    openAction->setEnabled(item != nullptr);
    deleteAction->setEnabled(item != nullptr && serverSync != nullptr);
    QAction* chosen = menu.exec(remoteCameraList->viewport()->mapToGlobal(pos));
    if (!chosen || !item)
        return;
    if (chosen == openAction) {
        handleRemoteCameraDoubleClick(item);
        return;
    }
    if (chosen == deleteAction && serverSync) {
        const QString cameraId = item->data(Qt::UserRole).toString();
        if (!cameraId.isEmpty())
            serverSync->deleteCameraRecord(cameraId);
    }
}

bool CamerasPage::cameraExistsOnServer(const QString& streamUrl) const
{
    return !remoteCameraIdForStream(streamUrl).isEmpty();
}

QString CamerasPage::remoteCameraIdForStream(const QString& streamUrl) const
{
    const QString normalized = streamUrl.trimmed().toLower();
    if (normalized.isEmpty())
        return {};
    for (const auto& camera : knownRemoteCameras) {
        if (camera.streamUrl.trimmed().toLower() == normalized)
            return camera.id;
    }
    return {};
}

CameraRecord CamerasPage::remoteCameraById(const QString& id) const
{
    if (id.isEmpty())
        return {};
    for (const auto& camera : knownRemoteCameras) {
        if (camera.id == id)
            return camera;
    }
    return {};
}
