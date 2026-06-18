#pragma once

#include "../../core/ThreeDViewerCamera.h"
#include "../../core/ThreeDViewerSceneModel.h"

#include <QObject>
#include <QPoint>

namespace TherionStudio
{

class ThreeDViewerViewportController final
    : public QObject
{
    Q_OBJECT

public:
    explicit ThreeDViewerViewportController(QObject *parent = nullptr);

    const ThreeDViewerCamera &camera() const;

    void fitToScene(const ThreeDViewerSceneModel &sceneModel, int viewportWidth = 0, int viewportHeight = 0);
    void resetView(const ThreeDViewerSceneModel &sceneModel, int viewportWidth = 0, int viewportHeight = 0);
    void setViewPreset(ThreeDViewerViewPreset preset,
                       const ThreeDViewerSceneModel &sceneModel,
                       int viewportWidth = 0,
                       int viewportHeight = 0);
    void rotateLeft();
    void rotateRight();
    void rotateByRadians(double radians);
    void adjustTiltDegrees(double deltaDegrees);
    void setFacingDegrees(double degrees);
    void setTiltDegrees(double degrees);
    void setDistanceMeters(double distanceMeters);
    void setFocalLengthMm(double focalLengthMm);

    bool mousePress(Qt::MouseButton button, const QPoint &position);
    bool mouseMove(const QPoint &position, int viewportHeight);
    bool mouseRelease(Qt::MouseButton button);
    bool wheel(const QPoint &angleDelta);

signals:
    void cameraChanged();

private:
    void emitCameraChanged();

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
