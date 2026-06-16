#pragma once

#include "../../core/ThreeDViewerCamera.h"

#include <QPointF>

namespace TherionStudio
{

struct ThreeDViewerProjectedPoint
{
    QPointF screenPosition;
    double depth = 0.0;
    bool visible = false;
};

class ThreeDViewerProjection
{
public:
    static ThreeDViewerProjectedPoint projectPoint(const ThreeDViewerCamera &camera,
                                                   const ThreeDViewerVec3 &point,
                                                   int viewportWidth,
                                                   int viewportHeight);
    static bool projectLine(const ThreeDViewerCamera &camera,
                            const ThreeDViewerVec3 &from,
                            const ThreeDViewerVec3 &to,
                            int viewportWidth,
                            int viewportHeight,
                            QPointF *fromScreen,
                            QPointF *toScreen);
};

} // namespace TherionStudio
