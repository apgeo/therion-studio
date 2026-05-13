#include "MapBackgroundPlacement.h"

#include <QtMath>

namespace TherionStudio
{
namespace
{
bool sizesNearlyEqual(const QSizeF &a, const QSizeF &b)
{
    if (!a.isValid() || !b.isValid()) {
        return false;
    }

    const qreal widthTolerance = qMax<qreal>(1.0, qMax(a.width(), b.width()) * 0.02);
    const qreal heightTolerance = qMax<qreal>(1.0, qMax(a.height(), b.height()) * 0.02);
    return qAbs(a.width() - b.width()) <= widthTolerance
        && qAbs(a.height() - b.height()) <= heightTolerance;
}
}

QRectF resolveRasterModelRect(const QSizeF &imageModelSize,
                              const RasterPlacementMetadata &metadata,
                              const AreaAdjustMetadata &areaAdjust)
{
    if (!imageModelSize.isValid() || imageModelSize.width() <= 0.0 || imageModelSize.height() <= 0.0 || !metadata.hasBasePosition) {
        return QRectF();
    }

    if (areaAdjust.valid && areaAdjust.modelRect.isValid() && sizesNearlyEqual(imageModelSize, areaAdjust.modelRect.size())) {
        return areaAdjust.modelRect;
    }

    const QPointF modelTopLeft(metadata.basePosition.x(),
                               metadata.topEdgeAnchor
                                   ? (metadata.basePosition.y() - imageModelSize.height())
                                   : metadata.basePosition.y());
    const QPointF modelBottomRight(modelTopLeft.x() + imageModelSize.width(),
                                   modelTopLeft.y() + imageModelSize.height());
    return QRectF(modelTopLeft, modelBottomRight);
}
}

