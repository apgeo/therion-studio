#pragma once

#include "../../core/ThreeDViewerCamera.h"
#include "../../core/ThreeDViewerSceneModel.h"

#include <QPoint>

namespace TherionStudio
{

class ThreeDViewerViewportController final
{
public:
    const ThreeDViewerCamera &camera() const;

    void fitToScene(const ThreeDViewerSceneModel &sceneModel);
    void resetView(const ThreeDViewerSceneModel &sceneModel);
    void setViewPreset(ThreeDViewerViewPreset preset, const ThreeDViewerSceneModel &sceneModel);
    void rotateLeft();
    void rotateRight();

    bool mousePress(Qt::MouseButton button, const QPoint &position);
    bool mouseMove(const QPoint &position, int viewportHeight);
    bool mouseRelease(Qt::MouseButton button);
    bool wheel(const QPoint &angleDelta);

private:
    enum class InteractionMode
    {
        None,
        Orbit,
        Pan,
    };

    ThreeDViewerCamera camera_;
    InteractionMode interactionMode_ = InteractionMode::None;
    QPoint lastMousePosition_;
    ThreeDViewerCameraState cameraAtPress_;
};

} // namespace TherionStudio
