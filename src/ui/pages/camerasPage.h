#ifndef CAMERASPAGE_H
#define CAMERASPAGE_H

/**
 * @file camerasPage.h
 * @brief UI page that manages a paginated grid of live camera tiles.
 *
 * Hosts multiple CameraViewWidget instances, wires frame updates from
 * CameraManager/AIProcessor, and exposes add/remove/toggle controls for streams
 * and audio.
 *
 * @example
 * auto* page = new CamerasPage(manager, processor, this);
 * stackedWidget->addWidget(page);
 */

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <atomic>
#include <QImage>
#include "../../core/CameraManager.h"
#include "../../core/AIProcessor.h"
#include "../../core/ServerTypes.h"
#include "../widgets/cameraViewWidget.h"

/**
 * @brief Displays camera tiles with controls to add/remove and paginate streams.
 */
class ServerSyncManager;
class QListWidget;
class QListWidgetItem;

class CamerasPage : public QWidget
{
    Q_OBJECT
public:
    /**
     * @brief Constructs the cameras page and binds frame signals.
     * @param manager Camera manager providing frames and audio control.
     * @param processor AI processor for annotated frames.
     * @param parent Optional parent widget.
     */
    explicit CamerasPage(CameraManager* manager, AIProcessor* processor, ServerSyncManager* sync, QWidget* parent = nullptr);

private slots:
    void onFrameReady(int id, const QImage& frame);
    void onFrameProcessed(int id, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);
    void onAddLocalCamera();
    void onAddRtspCamera();
    void onToggleCamera(int id, bool enable);
    void onRemoveCamera(int id);
    void onNextPage();
    void onPrevPage();
    void handleRemoteCamerasUpdated(const QList<CameraRecord>& cameras);
    void handleRemoteCameraDoubleClick(QListWidgetItem* item);
    void handleRemoteListContextMenu(const QPoint& pos);

private:
    CameraManager* cameraManager;
    ServerSyncManager* serverSync = nullptr;
    AIProcessor* aiProcessor = nullptr;
    struct CameraProcessingState {
        bool pending = false;
        bool hasAnnotated = false;
        QImage latestRawFrame;
    };
    QHash<int, CameraProcessingState> processingStates;
    QHash<int, CameraViewWidget*> cameraWidgets;
    QHash<int, CameraProcessingState> cameraStates;
    QHash<int, QString> cameraSources;
    QList<int> cameraOrder;

    QWidget* gridContainer = nullptr;
    QGridLayout* grid = nullptr;
    QPushButton* btnPrev = nullptr;
    QPushButton* btnNext = nullptr;
    QLineEdit* rtspInput = nullptr;
    QPushButton* btnAddRtsp = nullptr;
    QSpinBox* indexSpin = nullptr;
    QPushButton* btnAddIndex = nullptr;
    QListWidget* remoteCameraList = nullptr;
    int pageSize = 6;
    int currentPage = 0;
    int camsPerPage = 6;
    int nextCameraId = 0;
    std::atomic_bool aiBusy{ false };
    mutable QMutex detectionMutex;
    QList<CameraRecord> knownRemoteCameras;

signals:
    void requestProcessFrame(int id, QImage frame);

private:
    void setupUi();
    void updateGrid();
    void addCameraSource(const QString& source, const QString& title, bool registerOnServer = true);
    void updateRemoteListView();
    void publishCameraToServer(const QString& name, const QString& streamUrl);
    void openRemoteCamera(const CameraRecord& record);
    bool cameraExistsOnServer(const QString& streamUrl) const;
    QString remoteCameraIdForStream(const QString& streamUrl) const;
    CameraRecord remoteCameraById(const QString& id) const;
};

#endif // CAMERASPAGE_H
