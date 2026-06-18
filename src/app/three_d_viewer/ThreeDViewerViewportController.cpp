#include "ThreeDViewerViewportController.h"

#include <cmath>

namespace TherionStudio
{

ThreeDViewerViewportController::ThreeDViewerViewportController(QObject *parent)
    : QObject(parent)
{
}

const ThreeDViewerCamera &ThreeDViewerViewportController::camera() const
{
    return camera_;
}

void ThreeDViewerViewportController::fitToScene(const ThreeDViewerSceneModel &sceneModel)
{
    camera_.fitToBounds(sceneModel.bounds());
    emitCameraChanged();
}

void ThreeDViewerViewportController::resetView(const ThreeDViewerSceneModel &sceneModel)
{
    camera_.resetToBounds(sceneModel.bounds());
    emitCameraChanged();
}

void ThreeDViewerViewportController::setViewPreset(ThreeDViewerViewPreset preset, const ThreeDViewerSceneModel &sceneModel)
{
    camera_.setViewPreset(preset);
    camera_.fitToBounds(sceneModel.bounds());
    emitCameraChanged();
}

void ThreeDViewerViewportController::rotateLeft()
{
    camera_.yawByRadians(3.14159265358979323846 / 12.0);
    emitCameraChanged();
}

void ThreeDViewerViewportController::rotateRight()
{
    camera_.yawByRadians(-3.14159265358979323846 / 12.0);
    emitCameraChanged();
}

void ThreeDViewerViewportController::rotateByRadians(double radians)
{
    if (radians == 0.0) {
        return;
    }

    camera_.yawByRadians(radians);
    emitCameraChanged();
}

bool ThreeDViewerViewportController::mousePress(Qt::MouseButton button, const QPoint &position)
{
    lastMousePosition_ = position;
    cameraAtPress_ = camera_.state();

    if (button == Qt::LeftButton) {
        interactionMode_ = InteractionMode::Orbit;
        return true;
    }
    if (button == Qt::RightButton || button == Qt::MiddleButton) {
        interactionMode_ = InteractionMode::Pan;
        return true;
    }
    return false;
}

bool ThreeDViewerViewportController::mouseMove(const QPoint &position, int viewportHeight)
{
    if (interactionMode_ == InteractionMode::None) {
        return false;
    }

    const QPoint delta = position - lastMousePosition_;
    camera_.setState(cameraAtPress_);
    if (interactionMode_ == InteractionMode::Orbit) {
        camera_.orbitByPixels(delta.x(), delta.y());
    } else if (interactionMode_ == InteractionMode::Pan) {
        camera_.panByPixels(delta.x(), delta.y(), viewportHeight);
    }
    emitCameraChanged();
    return true;
}

bool ThreeDViewerViewportController::mouseRelease(Qt::MouseButton button)
{
    if (button == Qt::LeftButton || button == Qt::RightButton || button == Qt::MiddleButton) {
        interactionMode_ = InteractionMode::None;
        return true;
    }
    return false;
}

bool ThreeDViewerViewportController::wheel(const QPoint &angleDelta)
{
    if (angleDelta.y() == 0) {
        return false;
    }

    const double factor = angleDelta.y() > 0 ? 0.88 : 1.0 / 0.88;
    camera_.zoomByFactor(factor);
    emitCameraChanged();
    return true;
}

void ThreeDViewerViewportController::emitCameraChanged()
{
    emit cameraChanged();
}

} // namespace TherionStudio
