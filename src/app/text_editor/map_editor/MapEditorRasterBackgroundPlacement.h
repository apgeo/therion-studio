#pragma once

#include <QPointF>
#include <QRectF>

class QGraphicsPixmapItem;
class QImage;

namespace TherionStudio
{

struct TherionAreaAdjust;
struct TherionBackgroundReference;

QRectF fittedMapEditorModelBoundsInPreview(const QRectF &modelBounds, const QRectF &previewBounds);
QPointF mapEditorModelToPreviewPoint(const QPointF &modelPoint, const QRectF &modelBounds, const QRectF &previewBounds);
QPointF mapEditorPreviewToModelPoint(const QPointF &previewPoint, const QRectF &modelBounds, const QRectF &previewBounds);
QRectF mapEditorRasterModelRectForItem(const QGraphicsPixmapItem *item,
                                       const QRectF &sourceBounds,
                                       const QRectF &previewBounds);
bool placeMapEditorRasterLayerByModelRect(QGraphicsPixmapItem *item,
                                          const QImage &sourceImage,
                                          const QRectF &modelRect,
                                          const QRectF &modelBounds,
                                          const QRectF &previewBounds);
bool placeMapEditorRasterLayerPlaceholderByModelRect(QGraphicsPixmapItem *item,
                                                     const QRectF &modelRect,
                                                     const QRectF &modelBounds,
                                                     const QRectF &previewBounds);
bool placeMapEditorRasterLayerFromMetadata(QGraphicsPixmapItem *item,
                                           const QImage &sourceImage,
                                           const TherionBackgroundReference &reference,
                                           const TherionAreaAdjust &areaAdjust,
                                           const QRectF &modelBounds,
                                           const QRectF &previewBounds);
bool placeMapEditorRasterLayerPlaceholderFromMetadata(QGraphicsPixmapItem *item,
                                                      const TherionBackgroundReference &reference,
                                                      const TherionAreaAdjust &areaAdjust,
                                                      const QRectF &modelBounds,
                                                      const QRectF &previewBounds);

}
