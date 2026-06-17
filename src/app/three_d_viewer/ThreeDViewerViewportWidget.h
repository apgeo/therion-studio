#pragma once

#include "../../core/ThreeDViewerSceneModel.h"
#include "ThreeDViewerMeshColorMode.h"
#include "ThreeDViewerViewportController.h"

#include <QQuickWidget>

#include <array>

namespace TherionStudio
{

class ThreeDViewerViewportItem;

class ThreeDViewerViewportWidget final : public QQuickWidget
{
    Q_OBJECT

public:
    explicit ThreeDViewerViewportWidget(QWidget *parent = nullptr);

    void setSceneModel(const ThreeDViewerSceneModel &sceneModel);
    void setLayerVisibility(const std::array<bool, 5> &layerVisibility);
    void setMeshColorMode(ThreeDViewerMeshColorMode meshColorMode);
    void setMeasurementMode(bool measurementMode);
    void fitToScene();
    void resetView();
    void setViewPreset(ThreeDViewerViewPreset preset);
    void rollLeft();
    void rollRight();

private:
    void syncRootItem();
    ThreeDViewerViewportItem *rootViewportItem() const;

    ThreeDViewerSceneModel sceneModel_;
    std::array<bool, 5> layerVisibility_ = {true, true, true, true, true};
    ThreeDViewerMeshColorMode meshColorMode_ = ThreeDViewerMeshColorMode::Survey;
    bool measurementMode_ = false;
    bool pendingFitToScene_ = false;
    bool pendingResetView_ = false;
    bool pendingRollLeft_ = false;
    bool pendingRollRight_ = false;
    bool hasPendingViewPreset_ = false;
    ThreeDViewerViewPreset pendingViewPreset_ = ThreeDViewerViewPreset::Isometric;
};

} // namespace TherionStudio
