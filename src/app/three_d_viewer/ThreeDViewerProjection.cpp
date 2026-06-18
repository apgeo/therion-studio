#include "ThreeDViewerProjection.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>

namespace TherionStudio
{
namespace
{
QVector3D toVector3D(const ThreeDViewerVec3 &value)
{
    return {float(value.x), float(value.y), float(value.z)};
}

} // namespace

ThreeDViewerProjectedPoint ThreeDViewerProjection::projectPoint(const ThreeDViewerCamera &camera,
                                                                const ThreeDViewerVec3 &point,
                                                                int viewportWidth,
                                                                int viewportHeight,
                                                                bool orthographic)
{
    ThreeDViewerProjectedPoint projected;
    if (viewportWidth <= 0 || viewportHeight <= 0) {
        return projected;
    }

    const QVector3D forward = toVector3D(camera.forwardVector());
    const QVector3D right = toVector3D(camera.rightVector());
    const QVector3D up = toVector3D(camera.upVector());
    const QVector3D delta = toVector3D(point) - toVector3D(camera.position());

    const double depth = double(QVector3D::dotProduct(delta, forward));
    if (depth <= 0.1) {
        return projected;
    }

    const double x = double(QVector3D::dotProduct(delta, right));
    const double y = double(QVector3D::dotProduct(delta, up));
    const double halfHeight = std::max(1.0, double(viewportHeight) * 0.5);
    const double halfWidth = std::max(1.0, double(viewportWidth) * 0.5);
    const double projectionDepth = orthographic ? camera.state().distance : depth;
    const double scale = halfHeight / (projectionDepth * std::tan(camera.fieldOfViewRadians() * 0.5));

    projected.screenPosition = QPointF(halfWidth + x * scale, halfHeight - y * scale);
    projected.depth = depth;
    projected.visible = true;
    return projected;
}

bool ThreeDViewerProjection::projectLine(const ThreeDViewerCamera &camera,
                                         const ThreeDViewerVec3 &from,
                                         const ThreeDViewerVec3 &to,
                                         int viewportWidth,
                                         int viewportHeight,
                                         QPointF *fromScreen,
                                         QPointF *toScreen,
                                         bool orthographic)
{
    const ThreeDViewerProjectedPoint projectedFrom = projectPoint(camera, from, viewportWidth, viewportHeight, orthographic);
    const ThreeDViewerProjectedPoint projectedTo = projectPoint(camera, to, viewportWidth, viewportHeight, orthographic);
    if (!projectedFrom.visible || !projectedTo.visible) {
        return false;
    }

    if (fromScreen != nullptr) {
        *fromScreen = projectedFrom.screenPosition;
    }
    if (toScreen != nullptr) {
        *toScreen = projectedTo.screenPosition;
    }
    return true;
}

} // namespace TherionStudio
