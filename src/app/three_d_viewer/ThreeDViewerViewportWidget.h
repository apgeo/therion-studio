#pragma once

#include "../../core/ThreeDViewerSceneModel.h"

#include <QColor>
#include <QPointF>
#include <QVector3D>
#include <QWidget>

#include <array>

class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace TherionStudio
{

class ThreeDViewerViewportWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ThreeDViewerViewportWidget(QWidget *parent = nullptr);

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel);
    void setLayerVisibility(const std::array<bool, 5> &layerVisibility);
    void fitToScene();
    void resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    enum class InteractionMode
    {
        None,
        Orbit,
        Pan,
    };

    struct CameraState
    {
        ThreeDViewerVec3 target;
        double yaw = -0.85;
        double pitch = 0.45;
        double distance = 120.0;
    };

    struct ProjectedPoint
    {
        QPointF screenPosition;
        double depth = 0.0;
        bool visible = false;
    };

    void paintScene(QPainter &painter);
    void paintEmptyState(QPainter &painter);
    void paintAxes(QPainter &painter);
    void paintSurfaces(QPainter &painter);
    void paintMeshes(QPainter &painter);
    void paintShots(QPainter &painter);
    void paintStations(QPainter &painter);
    void paintLabels(QPainter &painter);
    ProjectedPoint projectPoint(const ThreeDViewerVec3 &point) const;
    bool projectLine(const ThreeDViewerVec3 &from, const ThreeDViewerVec3 &to, QPointF *fromScreen, QPointF *toScreen) const;
    QVector3D cameraPosition() const;
    QVector3D cameraRightVector() const;
    QVector3D cameraUpVector() const;
    QVector3D cameraForwardVector() const;
    QVector3D worldUpVector() const;
    void recenterCameraOnScene();
    double screenPanScale() const;
    static double clampPitch(double pitch);
    static QColor shotColorForFlags(const ThreeDViewerShot &shot);
    static QColor stationColorForFlags(const ThreeDViewerStation &station);

    ThreeDViewerSceneModel sceneModel_;
    std::array<bool, 5> layerVisibility_ = {true, true, true, true, true};
    CameraState camera_;
    InteractionMode interactionMode_ = InteractionMode::None;
    QPoint lastMousePosition_;
    CameraState cameraAtPress_;
};

} // namespace TherionStudio
