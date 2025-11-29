#ifndef CAMERASPAGE_H
#define CAMERASPAGE_H

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

class ServerSyncManager;
class QListWidget;

class CamerasPage : public QWidget
{
    Q_OBJECT
public:
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

private:
    CameraManager* cameraManager;
    ServerSyncManager* serverSync = nullptr;
    AIProcessor* aiProcessor = nullptr;
    struct CameraProcessingState {
        bool pending = false;
        bool hasAnnotated = false;
    };

    QHash<int, CameraViewWidget*> cameraWidgets;
    QHash<int, CameraProcessingState> cameraStates;
    QList<int> cameraOrder;

    QWidget* gridContainer;
    QGridLayout* grid;
    QPushButton* btnPrev;
    QPushButton* btnNext;
    QLineEdit* rtspInput;
    QPushButton* btnAddRtsp;
    QSpinBox* indexSpin = nullptr;
    QPushButton* btnAddIndex = nullptr;
    QListWidget* remoteCameraList = nullptr;

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
    void addCameraSource(const QString& source, const QString& title);
    void updateRemoteListView();
    void publishCameraToServer(const QString& name, const QString& streamUrl);
};

#endif // CAMERASPAGE_H
