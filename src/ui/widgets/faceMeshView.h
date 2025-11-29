#ifndef FACEMESHVIEW_H
#define FACEMESHVIEW_H

#include <QVector>
#include <QVector3D>
#include <QWidget>

namespace Qt3DCore {
class QEntity;
class QTransform;
}

namespace Qt3DExtras {
class Qt3DWindow;
class QOrbitCameraController;
}

namespace Qt3DRender {
class QCamera;
}

class FaceMeshView : public QWidget
{
    Q_OBJECT

public:
    explicit FaceMeshView(QWidget* parent = nullptr);
    void setPoints(const QVector<QVector3D>& points);
    void clear();

private:
    void rebuildPoints(const QVector<QVector3D>& points);
    void resetCamera(const QVector<QVector3D>& points);
    void createAxes();
    void clearPointEntities();

    Qt3DExtras::Qt3DWindow* view = nullptr;
    QWidget* container = nullptr;
    Qt3DCore::QEntity* rootEntity = nullptr;
    Qt3DCore::QEntity* axesRoot = nullptr;
    Qt3DCore::QEntity* pointsRoot = nullptr;
    Qt3DRender::QCamera* camera = nullptr;
    Qt3DExtras::QOrbitCameraController* orbitController = nullptr;
    QVector<Qt3DCore::QEntity*> pointEntities;
};

#endif // FACEMESHVIEW_H
