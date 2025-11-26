#ifndef CAMERASPAGE_H
#define CAMERASPAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QHash>
#include <QMutex>
#include <atomic>
#include <QImage>
#include "../../core/CameraManager.h"
#include "../../core/AIProcessor.h"
#include "../widgets/cameraViewWidget.h"

struct CameraDetectionState {
    QVector<Detection> detections;
    QSize sourceSize;
    bool pending = false;
};

class CamerasPage : public QWidget
{
    Q_OBJECT
public:
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
    QHash<int, CameraViewWidget*> cameraWidgets;
    QHash<int, CameraDetectionState> detectionStates;
    QList<int> cameraOrder;

    QWidget* gridContainer;
    QGridLayout* grid;
    QPushButton* btnPrev;
    QPushButton* btnNext;
    QLineEdit* rtspInput;
    QPushButton* btnAddRtsp;
    QSpinBox* indexSpin = nullptr;
    QPushButton* btnAddIndex = nullptr;

    int currentPage = 0;
    int camsPerPage = 6;
    int nextCameraId = 0;
    std::atomic_bool aiBusy{ false };
    mutable QMutex detectionMutex;

signals:
    void requestProcessFrame(int id, QImage frame);

private:
    void setupUi();
    void updateGrid();
    void addCameraSource(const QString& source, const QString& title);
    cv::Mat imageToMat(const QImage& image) const;
    QImage matToImage(const cv::Mat& mat) const;
    QImage composeFrame(const QImage& frame, const QVector<Detection>& detections, const QSize& sourceSize, const QSize& targetSize) const;
};

#endif // CAMERASPAGE_H
