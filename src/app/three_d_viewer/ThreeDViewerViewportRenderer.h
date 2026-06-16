#pragma once

#include "../../core/ThreeDViewerCamera.h"
#include "../../core/ThreeDViewerSceneModel.h"

#include <QPalette>
#include <QRect>

#include <array>

class QPainter;

namespace TherionStudio
{

class ThreeDViewerViewportRenderer final
{
public:
    static void paintEmptyState(QPainter &painter, const QRect &bounds, const QPalette &palette);
    static void paintScene(QPainter &painter,
                           const ThreeDViewerSceneModel &sceneModel,
                           const ThreeDViewerCamera &camera,
                           const std::array<bool, 5> &layerVisibility,
                           const QPalette &palette,
                           int viewportWidth,
                           int viewportHeight);
};

} // namespace TherionStudio
