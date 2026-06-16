#pragma once

#include "../../core/ThreeDViewerCamera.h"
#include "../../core/ThreeDViewerSceneModel.h"
#include "ThreeDViewerProjection.h"

#include <QColor>
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
    void setViewPreset(ThreeDViewerViewPreset preset);
    void rollLeft();
    void rollRight();

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

    void paintScene(QPainter &painter);
    void paintEmptyState(QPainter &painter);
    void paintAxes(QPainter &painter);
    void paintSurfaces(QPainter &painter);
    void paintMeshes(QPainter &painter);
    void paintShots(QPainter &painter);
    void paintStations(QPainter &painter);
    void paintLabels(QPainter &painter);
    static QColor shotColorForFlags(const ThreeDViewerShot &shot);
    static QColor stationColorForFlags(const ThreeDViewerStation &station);

    ThreeDViewerSceneModel sceneModel_;
    std::array<bool, 5> layerVisibility_ = {true, true, true, true, true};
    ThreeDViewerCamera camera_;
    InteractionMode interactionMode_ = InteractionMode::None;
    QPoint lastMousePosition_;
    ThreeDViewerCameraState cameraAtPress_;
};

} // namespace TherionStudio
