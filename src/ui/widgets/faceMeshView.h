#ifndef FACEMESHVIEW_H
#define FACEMESHVIEW_H

/**
 * @file faceMeshView.h
 * @brief Qt3D widget for visualizing 3D face landmark clouds.
 *
 * Renders a point cloud with colored axes and orbit camera controls. Exposes
 * methods to update the set of points, clear the view, and automatically frame
 * the camera on the current dataset.
 *
 * @example
 * auto* meshView = new FaceMeshView(this);
 * meshView->setPoints(landmarks);
 * layout->addWidget(meshView);
 */

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

/**
 * @brief 3D viewer that displays face mesh points with orbiting camera controls.
 *
 * Manages Qt3D entities for axes and points, normalizes coordinates for consistent
 * scaling, and repositions the camera to keep the mesh centered.
 */
class FaceMeshView : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the 3D view and initializes camera, lighting, and axes.
     * @param parent Optional parent widget.
     * @throws std::bad_alloc If Qt3D components cannot be created.
     * @example FaceMeshView* view = new FaceMeshView();
     */
    explicit FaceMeshView(QWidget* parent = nullptr);
    /**
     * @brief Replaces the currently rendered point cloud.
     * @param points Collection of 3D points to visualize.
     * @return void
     * @throws None
     * @example setPoints(landmarkPoints);
     */
    void setPoints(const QVector<QVector3D>& points);
    /**
     * @brief Clears all rendered points from the scene.
     * @return void
     * @throws None
     * @example clear();
     */
    void clear();

private:
    /**
     * @brief Rebuilds the point entities with normalized scaling.
     * @param points New point cloud.
     * @return void
     * @throws None
     * @example rebuildPoints(points);
     */
    void rebuildPoints(const QVector<QVector3D>& points);
    /**
     * @brief Recenters and positions the camera to fit the provided points.
     * @param points Point cloud used to compute bounds.
     * @return void
     * @throws None
     * @example resetCamera(points);
     */
    void resetCamera(const QVector<QVector3D>& points);
    /**
     * @brief Creates RGB axes helpers at the origin.
     * @return void
     * @throws None
     * @example createAxes();
     */
    void createAxes();
    /**
     * @brief Deletes existing point entities to free resources.
     * @return void
     * @throws None
     * @example clearPointEntities();
     */
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
