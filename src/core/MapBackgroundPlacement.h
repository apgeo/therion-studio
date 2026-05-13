#pragma once

#include <QPointF>
#include <QRectF>
#include <QSizeF>

namespace TherionStudio
{
struct RasterPlacementMetadata
{
    QPointF basePosition;
    bool hasBasePosition = false;
    bool topEdgeAnchor = true;
};

struct AreaAdjustMetadata
{
    QRectF modelRect;
    bool valid = false;
};

QRectF resolveRasterModelRect(const QSizeF &imageModelSize,
                              const RasterPlacementMetadata &metadata,
                              const AreaAdjustMetadata &areaAdjust);
}

