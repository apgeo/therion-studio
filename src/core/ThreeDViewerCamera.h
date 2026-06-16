#pragma once

#include "ThreeDViewerSceneModel.h"

namespace TherionStudio
{

struct ThreeDViewerCameraState
{
    ThreeDViewerVec3 target;
    double yaw = -0.85;
    double pitch = 0.45;
    double distance = 120.0;
};

class ThreeDViewerCamera
{
public:
    const ThreeDViewerCameraState &state() const;
    void setState(const ThreeDViewerCameraState &state);

    void fitToBounds(const ThreeDViewerBounds &bounds);
    void resetToBounds(const ThreeDViewerBounds &bounds);

    void orbitByPixels(double deltaX,
                       double deltaY,
                       double yawScale = 0.008,
                       double pitchScale = 0.008);
    void panByPixels(double deltaX,
                     double deltaY,
                     int viewportHeight,
                     double fieldOfViewRadians = defaultFieldOfViewRadians());
    void zoomByFactor(double factor);

    ThreeDViewerVec3 position() const;
    ThreeDViewerVec3 forwardVector() const;
    ThreeDViewerVec3 rightVector() const;
    ThreeDViewerVec3 upVector() const;

    double screenPanScale(int viewportHeight, double fieldOfViewRadians = defaultFieldOfViewRadians()) const;

    static double clampPitch(double pitch);
    static constexpr double defaultFieldOfViewRadians()
    {
        return 35.0 * 3.14159265358979323846 / 180.0;
    }

private:
    static constexpr double minDistance()
    {
        return 4.0;
    }

    static constexpr double maxDistance()
    {
        return 100000.0;
    }

    ThreeDViewerCameraState state_;
};

} // namespace TherionStudio
