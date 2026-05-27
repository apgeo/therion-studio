#include "MapEditorLineDecorationItem.h"

#include "MapEditorPointSymbolGeometry.h"

#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QStyleOptionGraphicsItem>
#include <QTransform>

#include <cmath>
#include <limits>
#include <optional>

namespace TherionStudio
{
namespace
{
qreal zoomOutStrokeScale(qreal lod)
{
    if (lod >= 1.0) {
        return 1.0;
    }

    return qBound<qreal>(0.32, std::pow(qMax<qreal>(0.01, lod), 0.72), 1.0);
}

quint32 stableHashU32(quint32 value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    value ^= value >> 16;
    return value;
}

qreal stableRandomSigned(quint32 seed, int major, int minor, quint32 salt = 0U)
{
    quint32 value = seed ^ (static_cast<quint32>(major) * 0x9e3779b9U) ^ (static_cast<quint32>(minor) * 0x85ebca6bU) ^ salt;
    value = stableHashU32(value);
    const qreal normalized = static_cast<qreal>(value) / static_cast<qreal>(std::numeric_limits<quint32>::max());
    return (normalized * 2.0) - 1.0;
}

qreal lineDecorationSideFactor(MapEditorLineDecorationSide side)
{
    switch (side) {
    case MapEditorLineDecorationSide::Left:
        return -1.0;
    case MapEditorLineDecorationSide::Right:
        return 1.0;
    case MapEditorLineDecorationSide::Center:
        return 0.0;
    }
    return 0.0;
}

qreal lineDecorationEffectiveOffset(const MapEditorLineDecorationStyle &decoration,
                                    int markerIndex,
                                    int seed)
{
    qreal offset = decoration.side == MapEditorLineDecorationSide::Center
        ? decoration.offset
        : lineDecorationSideFactor(decoration.side) * decoration.offset;
    if (decoration.offsetJitter > 0.0) {
        offset += stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x91f2U) * decoration.offsetJitter;
    }
    return offset;
}

qreal lineDecorationPadding(const QVector<MapEditorLineDecorationStyle> &decorations)
{
    qreal padding = 1.0;
    for (const MapEditorLineDecorationStyle &decoration : decorations) {
        qreal offsetExtent = std::abs(decoration.offset);
        for (const qreal offset : decoration.offsets) {
            offsetExtent = qMax(offsetExtent, std::abs(offset));
        }
        if (decoration.fromOffset.has_value()) {
            offsetExtent = qMax(offsetExtent, std::abs(decoration.fromOffset.value()));
        }
        if (decoration.toOffset.has_value()) {
            offsetExtent = qMax(offsetExtent, std::abs(decoration.toOffset.value()));
        }
        padding = qMax(padding,
                       offsetExtent
                           + decoration.size
                           + decoration.length
                           + decoration.sizeJitter
                           + decoration.offsetJitter
                           + decoration.strokeWidth
                           + 4.0);
    }
    return padding;
}

QPen lineDecorationPen(const MapEditorLineDecorationStyle &decoration,
                       const QColor &fallbackColor,
                       qreal zoomOutScale)
{
    QColor color = decoration.strokeColor.value_or(fallbackColor);
    QPen pen(color,
             qMax<qreal>(0.15, decoration.strokeWidth * zoomOutScale),
             decoration.strokeStyle,
             Qt::RoundCap,
             Qt::RoundJoin);
    pen.setCosmetic(true);
    if (!decoration.dashPattern.isEmpty()) {
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(decoration.dashPattern);
    }
    return pen;
}

struct LineDecorationSample
{
    QPointF point;
    QPointF tangent;
    QPointF normal;
};

std::optional<LineDecorationSample> lineDecorationSampleAt(const QPainterPath &path,
                                                           qreal distance,
                                                           bool reversed)
{
    const qreal pathLength = path.length();
    if (pathLength <= 0.001) {
        return std::nullopt;
    }

    const qreal clampedDistance = qBound<qreal>(0.0, distance, pathLength);
    const qreal percent = path.percentAtLength(clampedDistance);
    const QPointF point = path.pointAtPercent(percent);
    const qreal tangentStep = qBound<qreal>(0.5, pathLength * 0.002, 4.0);
    const QPointF before = path.pointAtPercent(path.percentAtLength(qMax<qreal>(0.0, clampedDistance - tangentStep)));
    const QPointF after = path.pointAtPercent(path.percentAtLength(qMin<qreal>(pathLength, clampedDistance + tangentStep)));
    QPointF tangent = after - before;
    const qreal tangentLength = std::hypot(tangent.x(), tangent.y());
    if (tangentLength <= 0.001) {
        return std::nullopt;
    }

    tangent /= tangentLength;
    if (reversed) {
        tangent = -tangent;
    }
    QPointF normal(tangent.y(), -tangent.x());
    const qreal normalLength = std::hypot(normal.x(), normal.y());
    if (normalLength <= 0.001) {
        return std::nullopt;
    }
    normal /= normalLength;
    return LineDecorationSample{point, tangent, normal};
}
}

MapEditorLineDecorationItem::MapEditorLineDecorationItem(const QPainterPath &path,
                                                         const QVector<MapEditorLineDecorationStyle> &decorations,
                                                         const QColor &fallbackColor,
                                                         bool reversed,
                                                         int fallbackSeed,
                                                         QGraphicsItem *parent)
    : QGraphicsPathItem(path, parent)
    , decorations_(decorations)
    , fallbackColor_(fallbackColor)
    , reversed_(reversed)
    , fallbackSeed_(fallbackSeed)
{
    setPen(Qt::NoPen);
    setBrush(Qt::NoBrush);
    updatePaintBounds();
}

void MapEditorLineDecorationItem::setDecorationPath(const QPainterPath &path)
{
    prepareGeometryChange();
    setPath(path);
    updatePaintBounds();
}

QRectF MapEditorLineDecorationItem::boundingRect() const
{
    return paintBounds_;
}

void MapEditorLineDecorationItem::paint(QPainter *painter,
                                        const QStyleOptionGraphicsItem *option,
                                        QWidget *widget)
{
    Q_UNUSED(widget);
    if (painter == nullptr || decorations_.isEmpty() || path().isEmpty()) {
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
    for (const MapEditorLineDecorationStyle &decoration : decorations_) {
        switch (decoration.kind) {
        case MapEditorLineDecorationKind::OffsetStroke:
            drawOffsetStroke(painter, decoration, decoration.offset, zoomOutScale);
            break;
        case MapEditorLineDecorationKind::Parallel: {
            const QVector<qreal> offsets = decoration.offsets.isEmpty()
                ? QVector<qreal>{decoration.offset}
                : decoration.offsets;
            for (const qreal offset : offsets) {
                drawOffsetStroke(painter, decoration, offset, zoomOutScale);
            }
            break;
        }
        case MapEditorLineDecorationKind::Ticks:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Ticks);
            break;
        case MapEditorLineDecorationKind::Rungs:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Rungs);
            break;
        case MapEditorLineDecorationKind::Teeth:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Teeth);
            break;
        case MapEditorLineDecorationKind::Symbols:
            drawRepeated(painter, decoration, zoomOutScale, DrawMode::Symbols);
            break;
        }
    }
    painter->restore();
}

void MapEditorLineDecorationItem::updatePaintBounds()
{
    const qreal padding = lineDecorationPadding(decorations_);
    paintBounds_ = path().controlPointRect().adjusted(-padding, -padding, padding, padding);
}

void MapEditorLineDecorationItem::drawOffsetStroke(QPainter *painter,
                                                   const MapEditorLineDecorationStyle &decoration,
                                                   qreal offset,
                                                   qreal zoomOutScale)
{
    const qreal pathLength = path().length();
    if (painter == nullptr || pathLength <= 0.001) {
        return;
    }

    QVector<QPointF> points;
    const qreal sampleStep = qBound<qreal>(3.0, decoration.spacing / 3.0, 12.0);
    const int sampleCount = qBound(2, static_cast<int>(std::ceil(pathLength / sampleStep)) + 1, 4096);
    points.reserve(sampleCount);
    for (int index = 0; index < sampleCount; ++index) {
        const qreal distance = index == sampleCount - 1
            ? pathLength
            : (pathLength * static_cast<qreal>(index) / static_cast<qreal>(sampleCount - 1));
        if (const std::optional<LineDecorationSample> sample = lineDecorationSampleAt(path(), distance, reversed_)) {
            points.append(sample->point + (sample->normal * offset));
        }
    }
    if (points.size() < 2) {
        return;
    }

    painter->setPen(lineDecorationPen(decoration, fallbackColor_, zoomOutScale));
    painter->setBrush(Qt::NoBrush);
    painter->drawPolyline(QPolygonF(points));
}

void MapEditorLineDecorationItem::drawRepeated(QPainter *painter,
                                               const MapEditorLineDecorationStyle &decoration,
                                               qreal zoomOutScale,
                                               DrawMode mode)
{
    const qreal pathLength = path().length();
    if (painter == nullptr || pathLength <= 0.001) {
        return;
    }

    const qreal spacing = qMax<qreal>(1.0, decoration.spacing);
    const int markerCount = qBound(0, static_cast<int>(std::floor(pathLength / spacing)), 2048);
    if (markerCount <= 0) {
        return;
    }

    painter->setPen(lineDecorationPen(decoration, fallbackColor_, zoomOutScale));
    for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
        const qreal distance = (markerIndex + 0.5) * spacing;
        const std::optional<LineDecorationSample> sample = lineDecorationSampleAt(path(), distance, reversed_);
        if (!sample.has_value()) {
            continue;
        }

        switch (mode) {
        case DrawMode::Ticks:
            drawTick(painter, decoration, sample->point, sample->normal, markerIndex);
            break;
        case DrawMode::Rungs:
            drawRung(painter, decoration, sample->point, sample->normal);
            break;
        case DrawMode::Teeth:
            drawTooth(painter, decoration, sample->point, sample->tangent, sample->normal, markerIndex);
            break;
        case DrawMode::Symbols:
            drawSymbol(painter, decoration, sample->point, sample->tangent, sample->normal, markerIndex, zoomOutScale);
            break;
        }
    }
}

void MapEditorLineDecorationItem::drawTick(QPainter *painter,
                                           const MapEditorLineDecorationStyle &decoration,
                                           const QPointF &point,
                                           const QPointF &normal,
                                           int markerIndex)
{
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const QPointF base = point + (normal * lineDecorationEffectiveOffset(decoration, markerIndex, seed));
    const qreal sideFactor = lineDecorationSideFactor(decoration.side);
    if (decoration.side == MapEditorLineDecorationSide::Center) {
        const QPointF half = normal * (decoration.length / 2.0);
        painter->drawLine(QLineF(base - half, base + half));
    } else {
        painter->drawLine(QLineF(base, base + (normal * sideFactor * decoration.length)));
    }
}

void MapEditorLineDecorationItem::drawRung(QPainter *painter,
                                           const MapEditorLineDecorationStyle &decoration,
                                           const QPointF &point,
                                           const QPointF &normal)
{
    const qreal fromOffset = decoration.fromOffset.value_or(-decoration.length / 2.0);
    const qreal toOffset = decoration.toOffset.value_or(decoration.length / 2.0);
    painter->drawLine(QLineF(point + (normal * fromOffset),
                             point + (normal * toOffset)));
}

void MapEditorLineDecorationItem::drawTooth(QPainter *painter,
                                            const MapEditorLineDecorationStyle &decoration,
                                            const QPointF &point,
                                            const QPointF &tangent,
                                            const QPointF &normal,
                                            int markerIndex)
{
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const QPointF baseCenter = point + (normal * lineDecorationEffectiveOffset(decoration, markerIndex, seed));
    const qreal sideFactor = decoration.side == MapEditorLineDecorationSide::Left ? -1.0 : 1.0;
    const qreal halfBase = decoration.size * 0.45;
    const QPointF base1 = baseCenter - (tangent * halfBase);
    const QPointF base2 = baseCenter + (tangent * halfBase);
    const QPointF tip = baseCenter + (normal * sideFactor * decoration.size);

    QPainterPath toothPath;
    toothPath.moveTo(base1);
    toothPath.lineTo(tip);
    toothPath.lineTo(base2);
    toothPath.closeSubpath();
    painter->setBrush(QBrush(decoration.fillColor.value_or(decoration.strokeColor.value_or(fallbackColor_))));
    painter->drawPath(toothPath);
    painter->setBrush(Qt::NoBrush);
}

void MapEditorLineDecorationItem::drawSymbol(QPainter *painter,
                                             const MapEditorLineDecorationStyle &decoration,
                                             const QPointF &point,
                                             const QPointF &tangent,
                                             const QPointF &normal,
                                             int markerIndex,
                                             qreal zoomOutScale)
{
    const int seed = decoration.seed.value_or(fallbackSeed_);
    const qreal sizeJitter = decoration.sizeJitter <= 0.0
        ? 0.0
        : stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x53acU) * decoration.sizeJitter;
    const qreal symbolSize = qBound<qreal>(0.6, decoration.size + sizeJitter, 128.0);
    const qreal offset = lineDecorationEffectiveOffset(decoration, markerIndex, seed);
    const QPointF center = point + (normal * offset);
    const QRectF symbolRect(center.x() - (symbolSize / 2.0),
                            center.y() - (symbolSize / 2.0),
                            symbolSize,
                            symbolSize);
    QPainterPath symbolPath = mapEditorPointSymbolPath(decoration.symbol, symbolRect);
    const qreal tangentAngle = std::atan2(tangent.y(), tangent.x()) * 180.0 / 3.14159265358979323846;
    const qreal jitterAngle = decoration.angleJitter <= 0.0
        ? 0.0
        : stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0xa174U) * decoration.angleJitter;
    QTransform transform;
    transform.translate(center.x(), center.y());
    transform.rotate(tangentAngle + decoration.angle + jitterAngle);
    transform.translate(-center.x(), -center.y());
    symbolPath = transform.map(symbolPath);

    const QColor strokeColor = decoration.strokeColor.value_or(fallbackColor_);
    if (mapEditorPointSymbolUsesFill(decoration.symbol)) {
        if (decoration.fillColor.has_value()) {
            painter->setBrush(QBrush(decoration.fillColor.value()));
        } else {
            painter->setBrush(Qt::NoBrush);
        }
    } else {
        painter->setBrush(Qt::NoBrush);
    }
    QPen pen(strokeColor,
             qMax<qreal>(0.15, decoration.strokeWidth * zoomOutScale),
             decoration.strokeStyle,
             Qt::RoundCap,
             Qt::RoundJoin);
    pen.setCosmetic(true);
    if (!decoration.dashPattern.isEmpty()) {
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(decoration.dashPattern);
    }
    painter->setPen(pen);
    painter->drawPath(symbolPath);
    painter->setBrush(Qt::NoBrush);
}
}
