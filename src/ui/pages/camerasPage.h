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
#include "../widgets/cameraViewWidget.h"

/**
 * @brief Displays camera tiles with controls to add/remove and paginate streams.
 */
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
    explicit CamerasPage(CameraManager* manager, AIProcessor* processor, QWidget* parent = nullptr);

private slots:
    void onFrameReady(int id, const QImage& frame);
    void onFrameProcessed(int id, const QImage& annotated, const QVector<Detection>& detections, const QSize& sourceSize);
    void onAddLocalCamera();
    void onAddRtspCamera();
    void onToggleCamera(int id, bool enable);
    void onRemoveCamera(int id);
    void onNextPage();
    void onPrevPage();

private:
    CameraManager* cameraManager;
    AIProcessor* aiProcessor = nullptr;
    struct CameraProcessingState {
        bool pending = false;
        bool hasAnnotated = false;
        QImage latestRawFrame;
    };
    QHash<int, CameraProcessingState> processingStates;
    QHash<int, CameraViewWidget*> cameraWidgets;

    QGridLayout* gridLayout = nullptr;
    QScrollArea* scrollArea = nullptr;
    QPushButton* addLocalButton = nullptr;
    QPushButton* addRtspButton = nullptr;
    QLineEdit* rtspUrlEdit = nullptr;
    QSpinBox* cameraIdSpin = nullptr;
    QPushButton* prevPageButton = nullptr;
    QPushButton* nextPageButton = nullptr;
    QWidget* gridContainer = nullptr;
    int pageSize = 6;
    int currentPage = 0;
    QSet<int> disabledCameras;
    QMutex mutex;
};

#endif // CAMERASPAGE_H
