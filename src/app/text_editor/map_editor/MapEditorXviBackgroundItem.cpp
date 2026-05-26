#include "MapEditorXviBackgroundItem.h"

#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

#include <cmath>

namespace TherionStudio
{
namespace
{
qreal zoomOutStrokeScale(qreal lod)
{
    if (lod >= 1.0) {
        return 1.0;
    }

    return qBound<qreal>(0.30, std::pow(qMax<qreal>(0.01, lod), 0.80), 1.0);
}
}

MapEditorXviBackgroundItem::MapEditorXviBackgroundItem(QGraphicsItem *parent)
    : QGraphicsPixmapItem(parent)
{
    setCacheMode(QGraphicsItem::NoCache);
}

void MapEditorXviBackgroundItem::setGeometryData(const MapEditorXviLayerGeometryData &geometry)
{
    prepareGeometryChange();
    geometry_ = geometry;
    paintBounds_ = geometry_.contentBounds.adjusted(-2.0, -2.0, 2.0, 2.0);
}

const MapEditorXviLayerGeometryData &MapEditorXviBackgroundItem::geometryData() const
{
    return geometry_;
}

QRectF MapEditorXviBackgroundItem::boundingRect() const
{
    return paintBounds_;
}

void MapEditorXviBackgroundItem::paint(QPainter *painter,
                                       const QStyleOptionGraphicsItem *option,
                                       QWidget *widget)
{
    Q_UNUSED(widget);

    if (painter == nullptr || !geometry_.hasContent()) {
        return;
    }
    if (option != nullptr && !option->exposedRect.intersects(paintBounds_)) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const qreal lod = option != nullptr
        ? QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform())
        : 1.0;
    const qreal zoomOutScale = zoomOutStrokeScale(lod);

    QPen gridPen(QColor(32, 32, 32, 40));
    gridPen.setWidthF(1.0 * zoomOutScale);
    gridPen.setCosmetic(true);
    gridPen.setCapStyle(Qt::RoundCap);
    painter->setPen(gridPen);
    if (!geometry_.gridPath.isEmpty() && lod >= 0.30) {
        painter->drawPath(geometry_.gridPath);
    }

    QPen traverseShotPen(QColor(18, 44, 88, 130));
    traverseShotPen.setWidthF(1.8 * zoomOutScale);
    traverseShotPen.setCosmetic(true);
    traverseShotPen.setCapStyle(Qt::RoundCap);
    traverseShotPen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(traverseShotPen);
    if (!geometry_.traverseShotPath.isEmpty()) {
        painter->drawPath(geometry_.traverseShotPath);
    }

    QPen splayPen(QColor(28, 92, 64, 122));
    splayPen.setWidthF(1.4 * zoomOutScale);
    splayPen.setCosmetic(true);
    splayPen.setCapStyle(Qt::RoundCap);
    splayPen.setJoinStyle(Qt::RoundJoin);
    painter->setPen(splayPen);
    if (!geometry_.splayShotPath.isEmpty() && lod >= 0.42) {
        painter->drawPath(geometry_.splayShotPath);
    }

    if (!geometry_.sketchPaths.isEmpty() && lod >= 0.60) {
        for (const MapEditorXviSketchPathData &sketch : geometry_.sketchPaths) {
            if (sketch.path.isEmpty()) {
                continue;
            }

            QColor strokeColor = sketch.color.isValid() ? sketch.color : QColor(0, 0, 0);
            if (strokeColor.alpha() > 96) {
                strokeColor.setAlpha(96);
            } else if (strokeColor.alpha() <= 0) {
                strokeColor.setAlpha(86);
            }

            QPen sketchPen(strokeColor);
            sketchPen.setStyle(sketch.style);
            sketchPen.setWidthF(1.15 * zoomOutScale);
            sketchPen.setCosmetic(true);
            sketchPen.setCapStyle(Qt::RoundCap);
            sketchPen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(sketchPen);
            painter->drawPath(sketch.path);
        }
    }

    painter->restore();
}
}
