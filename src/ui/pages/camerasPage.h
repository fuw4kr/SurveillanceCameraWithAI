#ifndef CAMERASPAGE_H
#define CAMERASPAGE_H

#include <QWidget>
#include <QGridLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QLineEdit>
#include "../../core/CameraManager.h"
#include "CameraViewWidget.h"

class CamerasPage : public QWidget
{
    Q_OBJECT
public:
    explicit CamerasPage(CameraManager* manager, QWidget* parent = nullptr);

private slots:
    void onFrameReady(int id, const QImage& frame);
    void onAddCameraClicked();
    void onToggleCamera(int id, bool enable);
    void onNextPage();
    void onPrevPage();

private:
    CameraManager* cameraManager;
    QList<CameraViewWidget*> cameraWidgets;

    QWidget* gridContainer;
    QGridLayout* grid;
    QPushButton* btnPrev;
    QPushButton* btnNext;
    QLineEdit* ipInput;
    QPushButton* btnAddCamera;

    int currentPage = 0;
    int camsPerPage = 9;

    void setupUi();
    void updateGrid();
};

#endif // CAMERASPAGE_H
