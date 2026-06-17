#pragma once

#include "../../core/ThreeDViewerSceneModel.h"
#include "ThreeDViewerMeshColorMode.h"
#include "ThreeDViewerViewportController.h"

#include <QMutex>
#include <QPointF>
#include <QQuickItem>

#include <array>

class QMouseEvent;
class QHoverEvent;
class QWheelEvent;
class QSGNode;

namespace TherionStudio
{

class ThreeDViewerViewportItem : public QQuickItem
{
    Q_OBJECT

public:
    explicit ThreeDViewerViewportItem(QQuickItem *parent = nullptr);

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel);
    void setLayerVisibility(const std::array<bool, 5> &layerVisibility);
    void setMeshColorMode(ThreeDViewerMeshColorMode meshColorMode);
    void setMeasurementMode(bool measurementMode);
    void fitToScene();
    void resetView();
    void setViewPreset(ThreeDViewerViewPreset preset);
    void rollLeft();
    void rollRight();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void hoverLeaveEvent(QHoverEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct Snapshot
    {
        ThreeDViewerSceneModel sceneModel;
        std::array<bool, 5> layerVisibility = {true, true, true, true, true};
        ThreeDViewerMeshColorMode meshColorMode = ThreeDViewerMeshColorMode::Survey;
        bool measurementMode = false;
        bool hasHoveredStation = false;
        quint32 hoveredStationId = 0;
        QPointF hoveredStationScreenPosition;
        bool hasMeasurementStartStation = false;
        quint32 measurementStartStationId = 0;
        bool hasMeasurementEndStation = false;
        quint32 measurementEndStationId = 0;
        ThreeDViewerCamera camera;
    };

    Snapshot snapshot() const;
    void scheduleUpdate();

    mutable QMutex mutex_;
    ThreeDViewerSceneModel sceneModel_;
    std::array<bool, 5> layerVisibility_ = {true, true, true, true, true};
    ThreeDViewerMeshColorMode meshColorMode_ = ThreeDViewerMeshColorMode::Survey;
    bool measurementMode_ = false;
    ThreeDViewerCamera cameraSnapshot_;
    ThreeDViewerViewportController controller_;
    quint32 hoveredStationId_ = 0;
    bool hasHoveredStation_ = false;
    QPointF hoveredStationScreenPosition_;
    quint32 measurementStartStationId_ = 0;
    bool hasMeasurementStartStation_ = false;
    quint32 measurementEndStationId_ = 0;
    bool hasMeasurementEndStation_ = false;
};

} // namespace TherionStudio
