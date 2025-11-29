#include "faceMeshView.h"

#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DExtras/QCylinderMesh>
#include <Qt3DExtras/QForwardRenderer>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QPointLight>

#include <QHBoxLayout>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <cmath>

namespace {
Qt3DCore::QEntity* createAxis(Qt3DCore::QEntity* parent, const QVector3D& direction, const QColor& color)
{
    auto* axisEntity = new Qt3DCore::QEntity(parent);

    auto* cylinder = new Qt3DExtras::QCylinderMesh(axisEntity);
    cylinder->setRadius(0.01f);
    cylinder->setLength(2.0f);
    cylinder->setRings(12);
    cylinder->setSlices(24);

    auto* transform = new Qt3DCore::QTransform(axisEntity);
    transform->setTranslation(direction.normalized());
    transform->setRotation(QQuaternion::rotationTo(QVector3D(0.0f, 1.0f, 0.0f), direction.normalized()));

    auto* material = new Qt3DExtras::QPhongMaterial(axisEntity);
    material->setDiffuse(color);

    axisEntity->addComponent(cylinder);
    axisEntity->addComponent(transform);
    axisEntity->addComponent(material);
    return axisEntity;
}
}

FaceMeshView::FaceMeshView(QWidget* parent)
    : QWidget(parent)
{
    view = new Qt3DExtras::Qt3DWindow;
    view->defaultFrameGraph()->setClearColor(QColor("#020617"));
    container = QWidget::createWindowContainer(view, this);
    container->setMinimumSize(QSize(400, 300));
    container->setFocusPolicy(Qt::StrongFocus);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(container);

    rootEntity = new Qt3DCore::QEntity;
    view->setRootEntity(rootEntity);

    camera = view->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0.0f, 0.0f, 4.0f));
    camera->setViewCenter(QVector3D(0.0f, 0.0f, 0.0f));

    orbitController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    orbitController->setCamera(camera);
    orbitController->setLinearSpeed(8.0f);
    orbitController->setLookSpeed(100.0f);

    auto* lightEntity = new Qt3DCore::QEntity(rootEntity);
    auto* pointLight = new Qt3DRender::QPointLight(lightEntity);
    pointLight->setColor(QColor("#f8fafc"));
    pointLight->setIntensity(120.0f);
    auto* lightTransform = new Qt3DCore::QTransform(lightEntity);
    lightTransform->setTranslation(QVector3D(10.0f, 10.0f, 10.0f));
    lightEntity->addComponent(pointLight);
    lightEntity->addComponent(lightTransform);

    pointsRoot = new Qt3DCore::QEntity(rootEntity);
    axesRoot = new Qt3DCore::QEntity(rootEntity);
    createAxes();
}

void FaceMeshView::createAxes()
{
    createAxis(axesRoot, QVector3D(1.0f, 0.0f, 0.0f), QColor("#ef4444"));
    createAxis(axesRoot, QVector3D(0.0f, 1.0f, 0.0f), QColor("#22c55e"));
    createAxis(axesRoot, QVector3D(0.0f, 0.0f, 1.0f), QColor("#3b82f6"));
}

void FaceMeshView::clearPointEntities()
{
    qDeleteAll(pointEntities);
    pointEntities.clear();
}

void FaceMeshView::clear()
{
    clearPointEntities();
    normalizedPoints.clear();
}

void FaceMeshView::setPoints(const QVector<QVector3D>& points)
{
    rebuildPoints(points);
    resetCamera();
}

void FaceMeshView::rebuildPoints(const QVector<QVector3D>& points)
{
    clearPointEntities();
    normalizedPoints.clear();
    if (!pointsRoot || points.isEmpty())
        return;

    QVector3D minPoint = points.first();
    QVector3D maxPoint = points.first();
    for (const QVector3D& p : points) {
        minPoint.setX(std::min(minPoint.x(), p.x()));
        minPoint.setY(std::min(minPoint.y(), p.y()));
        minPoint.setZ(std::min(minPoint.z(), p.z()));
        maxPoint.setX(std::max(maxPoint.x(), p.x()));
        maxPoint.setY(std::max(maxPoint.y(), p.y()));
        maxPoint.setZ(std::max(maxPoint.z(), p.z()));
    }

    const QVector3D center = (minPoint + maxPoint) * 0.5f;
    const float spanX = maxPoint.x() - minPoint.x();
    const float spanY = maxPoint.y() - minPoint.y();
    const float spanZ = maxPoint.z() - minPoint.z();
    const float span = std::max({ spanX, spanY, std::max(spanZ, 1.0f) });
    const float scale = span > 0.0f ? (2.0f / span) : 1.0f;
    const int sampleCount = std::max(1, static_cast<int>(points.size()));
    const float density = std::pow(static_cast<float>(sampleCount), 1.0f / 3.0f);
    const float radius = std::max(0.008f, 0.12f / std::max(1.0f, density));

    normalizedPoints.reserve(points.size());

    for (const QVector3D& rawPoint : points) {
        QVector3D normalized = (rawPoint - center) * scale;
        normalizedPoints.append(normalized);

        auto* entity = new Qt3DCore::QEntity(pointsRoot);
        auto* mesh = new Qt3DExtras::QSphereMesh(entity);
        mesh->setRadius(radius);
        mesh->setRings(16);
        mesh->setSlices(16);

        auto* transform = new Qt3DCore::QTransform(entity);
        transform->setTranslation(normalized);

        auto* material = new Qt3DExtras::QPhongMaterial(entity);
        material->setDiffuse(QColor("#4ade80"));

        entity->addComponent(mesh);
        entity->addComponent(transform);
        entity->addComponent(material);
        pointEntities.append(entity);
    }
}

void FaceMeshView::resetCamera()
{
    if (!camera || normalizedPoints.isEmpty())
        return;

    QVector3D minPoint = normalizedPoints.first();
    QVector3D maxPoint = normalizedPoints.first();
    for (const QVector3D& p : normalizedPoints) {
        minPoint.setX(std::min(minPoint.x(), p.x()));
        minPoint.setY(std::min(minPoint.y(), p.y()));
        minPoint.setZ(std::min(minPoint.z(), p.z()));
        maxPoint.setX(std::max(maxPoint.x(), p.x()));
        maxPoint.setY(std::max(maxPoint.y(), p.y()));
        maxPoint.setZ(std::max(maxPoint.z(), p.z()));
    }

    const QVector3D center = (minPoint + maxPoint) * 0.5f;
    const float spanX = maxPoint.x() - minPoint.x();
    const float spanY = maxPoint.y() - minPoint.y();
    const float spanZ = maxPoint.z() - minPoint.z();
    const float span = std::max({ spanX, spanY, spanZ, 1.0f });
    const float distance = std::max(3.0f, span * 1.5f);

    camera->setPosition(center + QVector3D(0.0f, 0.0f, distance));
    camera->setViewCenter(center);
}
