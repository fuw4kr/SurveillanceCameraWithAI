#ifndef ANALYTICSPAGE_H
#define ANALYTICSPAGE_H

#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>

#include "../../core/cameraManager.h"
#include "../../core/AIProcessor.h"

class AnalyticsPage : public QWidget
{
    Q_OBJECT
public:
    explicit AnalyticsPage(CameraManager* manager, AIProcessor* processor, QWidget* parent = nullptr);

private slots:
    void refreshCameras();
    void onCameraChanged(int index);
    void handleFrame(int id, const QImage& frame);

private:
    void buildUi();
    void updateDetections(const QVector<Detection>& detections);
    void updateStats(const QVector<Detection>& detections);
    cv::Mat imageToMat(const QImage& image) const;
    QImage matToImage(const cv::Mat& mat) const;

    CameraManager* cameraManager = nullptr;
    AIProcessor* aiProcessor = nullptr;

    QComboBox* cameraCombo = nullptr;
    QLabel* previewLabel = nullptr;
    QLabel* statsLabel = nullptr;
    QListWidget* detectionList = nullptr;

    int currentCameraId = -1;
};

#endif // ANALYTICSPAGE_H
