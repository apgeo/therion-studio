#include "ThreeDViewerCamera.h"

#include <algorithm>
#include <cmath>

namespace TherionStudio
{
namespace
{
constexpr ThreeDViewerVec3 worldUp{0.0, 0.0, 1.0};

double vectorLengthSquared(const ThreeDViewerVec3 &value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

double vectorLength(const ThreeDViewerVec3 &value)
{
    return std::sqrt(vectorLengthSquared(value));
}

ThreeDViewerVec3 subtract(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

ThreeDViewerVec3 add(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

ThreeDViewerVec3 multiply(const ThreeDViewerVec3 &value, double factor)
{
    return {value.x * factor, value.y * factor, value.z * factor};
}

ThreeDViewerVec3 cross(const ThreeDViewerVec3 &left, const ThreeDViewerVec3 &right)
{
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

ThreeDViewerVec3 normalizedOrFallback(const ThreeDViewerVec3 &value, const ThreeDViewerVec3 &fallback)
{
    const double length = vectorLength(value);
    if (length <= 0.0001) {
        return fallback;
    }
    return {value.x / length, value.y / length, value.z / length};
}
} // namespace

const ThreeDViewerCameraState &ThreeDViewerCamera::state() const
{
    return state_;
}

void ThreeDViewerCamera::setState(const ThreeDViewerCameraState &state)
{
    state_ = state;
    state_.pitch = clampPitch(state_.pitch);
    state_.distance = std::clamp(state_.distance, minDistance(), maxDistance());
}

void ThreeDViewerCamera::fitToBounds(const ThreeDViewerBounds &bounds)
{
    if (!bounds.valid) {
        state_.target = {0.0, 0.0, 0.0};
        state_.distance = 120.0;
        return;
    }

    state_.target = {(bounds.minimum.x + bounds.maximum.x) * 0.5,
                     (bounds.minimum.y + bounds.maximum.y) * 0.5,
                     (bounds.minimum.z + bounds.maximum.z) * 0.5};

    const ThreeDViewerVec3 diagonal = subtract(bounds.maximum, bounds.minimum);
    const double radius = std::max(1.0, vectorLength(diagonal) * 0.5);
    const double distance = radius / std::tan(defaultFieldOfViewRadians() * 0.5) * 1.35;
    state_.distance = std::clamp(distance, minDistance(), maxDistance());
}

void ThreeDViewerCamera::resetToBounds(const ThreeDViewerBounds &bounds)
{
    state_.yaw = -0.85;
    state_.pitch = 0.45;
    fitToBounds(bounds);
}

void ThreeDViewerCamera::setViewPreset(ThreeDViewerViewPreset preset)
{
    switch (preset) {
    case ThreeDViewerViewPreset::Isometric:
        state_.yaw = -0.85;
        state_.pitch = 0.45;
        break;
    case ThreeDViewerViewPreset::Top:
        state_.yaw = -0.85;
        state_.pitch = 1.34;
        break;
    case ThreeDViewerViewPreset::Side:
        state_.yaw = 0.0;
        state_.pitch = 0.0;
        break;
    }
    state_.pitch = clampPitch(state_.pitch);
}

void ThreeDViewerCamera::yawByRadians(double deltaRadians)
{
    state_.yaw = std::remainder(state_.yaw + deltaRadians, 2.0 * 3.14159265358979323846);
}

void ThreeDViewerCamera::orbitByPixels(double deltaX, double deltaY, double yawScale, double pitchScale)
{
    state_.yaw += deltaX * yawScale;
    state_.pitch = clampPitch(state_.pitch + deltaY * pitchScale);
}

void ThreeDViewerCamera::panByPixels(double deltaX,
                                     double deltaY,
                                     int viewportHeight,
                                     double fieldOfViewRadians)
{
    if (viewportHeight <= 0) {
        return;
    }

    const ThreeDViewerVec3 right = rightVector();
    const ThreeDViewerVec3 up = upVector();
    const double scale = screenPanScale(viewportHeight, fieldOfViewRadians);
    state_.target = subtract(state_.target,
                             add(multiply(right, deltaX * scale), multiply(up, -deltaY * scale)));
}

void ThreeDViewerCamera::zoomByFactor(double factor)
{
    if (!(factor > 0.0)) {
        return;
    }
    state_.distance = std::clamp(state_.distance * factor, minDistance(), maxDistance());
}

ThreeDViewerVec3 ThreeDViewerCamera::position() const
{
    const double cosPitch = std::cos(state_.pitch);
    return add(state_.target,
               {state_.distance * cosPitch * std::cos(state_.yaw),
                state_.distance * cosPitch * std::sin(state_.yaw),
                state_.distance * std::sin(state_.pitch)});
}

ThreeDViewerVec3 ThreeDViewerCamera::forwardVector() const
{
    const ThreeDViewerVec3 forward = subtract(state_.target, position());
    return normalizedOrFallback(forward, {0.0, 0.0, -1.0});
}

ThreeDViewerVec3 ThreeDViewerCamera::rightVector() const
{
    const ThreeDViewerVec3 forward = forwardVector();
    ThreeDViewerVec3 right = cross(forward, worldUp);
    if (vectorLengthSquared(right) <= 0.00000001) {
        right = cross(forward, {0.0, 1.0, 0.0});
    }
    right = normalizedOrFallback(right, {1.0, 0.0, 0.0});
    return right;
}

ThreeDViewerVec3 ThreeDViewerCamera::upVector() const
{
    const ThreeDViewerVec3 forward = forwardVector();
    const ThreeDViewerVec3 right = rightVector();
    return normalizedOrFallback(cross(right, forward), worldUp);
}

double ThreeDViewerCamera::screenPanScale(int viewportHeight, double fieldOfViewRadians) const
{
    if (viewportHeight <= 0) {
        return 0.0;
    }

    return 2.0 * state_.distance * std::tan(fieldOfViewRadians * 0.5) / double(viewportHeight);
}

double ThreeDViewerCamera::clampPitch(double pitch)
{
    return std::clamp(pitch, -1.45, 1.45);
}

} // namespace TherionStudio
