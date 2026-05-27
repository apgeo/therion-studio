#include "MapEditorStylePreviewWidget.h"

#include "MapEditorObjectStyleCatalog.h"
#include "MapEditorPointSymbolGeometry.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>
#include <QPolygonF>
#include <QLineF>
#include <QSizePolicy>
#include <QTransform>

#include <cmath>
#include <limits>
#include <optional>

namespace TherionStudio
{
namespace
{
QPen previewPen(const QColor &color,
                qreal width,
                Qt::PenStyle style,
                const QVector<qreal> &dashPattern = {})
{
    QPen pen(color, qMax<qreal>(0.6, width), style, Qt::RoundCap, Qt::RoundJoin);
    pen.setCosmetic(true);
    if (!dashPattern.isEmpty()) {
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(dashPattern);
    }
    return pen;
}

qreal previewLinearComponent(qreal component)
{
    return component <= 0.03928
        ? component / 12.92
        : std::pow((component + 0.055) / 1.055, 2.4);
}

qreal previewRelativeLuminance(const QColor &color)
{
    return (0.2126 * previewLinearComponent(color.redF()))
        + (0.7152 * previewLinearComponent(color.greenF()))
        + (0.0722 * previewLinearComponent(color.blueF()));
}

qreal previewContrastRatio(const QColor &a, const QColor &b)
{
    const qreal light = qMax(previewRelativeLuminance(a), previewRelativeLuminance(b));
    const qreal dark = qMin(previewRelativeLuminance(a), previewRelativeLuminance(b));
    return (light + 0.05) / (dark + 0.05);
}

QColor readablePreviewColor(const QColor &requestedColor,
                            const QColor &backgroundColor,
                            const QColor &fallbackColor)
{
    if (!requestedColor.isValid()) {
        return fallbackColor;
    }
    if (previewContrastRatio(requestedColor, backgroundColor) >= 2.4) {
        return requestedColor;
    }
    return previewContrastRatio(fallbackColor, backgroundColor) > previewContrastRatio(requestedColor, backgroundColor)
        ? fallbackColor
        : requestedColor;
}

QColor previewTileBackgroundColor(const QPalette &palette)
{
    const QColor base = palette.color(QPalette::Base);
    if (previewRelativeLuminance(base) >= 0.55) {
        return base;
    }
    return QColor(248, 248, 245);
}

bool needsPreviewContrastUnderlay(const QColor &requestedColor,
                                  const QColor &backgroundColor)
{
    return requestedColor.isValid()
        && previewContrastRatio(requestedColor, backgroundColor) < 2.4;
}

QColor previewContrastUnderlayColor(const QColor &requestedColor)
{
    const QColor white(Qt::white);
    const QColor black(Qt::black);
    QColor underlay = previewContrastRatio(white, requestedColor) >= previewContrastRatio(black, requestedColor)
        ? white
        : black;
    underlay.setAlpha(185);
    return underlay;
}

void drawPreviewPolyline(QPainter *painter,
                         const QPolygonF &points,
                         const QColor &strokeColor,
                         qreal strokeWidth,
                         Qt::PenStyle strokeStyle,
                         const QVector<qreal> &dashPattern,
                         const QColor &backgroundColor)
{
    if (painter == nullptr || points.size() < 2) {
        return;
    }

    painter->setBrush(Qt::NoBrush);
    if (needsPreviewContrastUnderlay(strokeColor, backgroundColor)) {
        painter->setPen(previewPen(previewContrastUnderlayColor(strokeColor),
                                   strokeWidth + 2.2,
                                   strokeStyle,
                                   dashPattern));
        painter->drawPolyline(points);
    }
    painter->setPen(previewPen(strokeColor, strokeWidth, strokeStyle, dashPattern));
    painter->drawPolyline(points);
}

void drawPreviewPathStroke(QPainter *painter,
                           const QPainterPath &path,
                           const QColor &strokeColor,
                           qreal strokeWidth,
                           Qt::PenStyle strokeStyle,
                           const QVector<qreal> &dashPattern,
                           const QColor &backgroundColor)
{
    if (painter == nullptr || path.isEmpty()) {
        return;
    }

    painter->setBrush(Qt::NoBrush);
    if (needsPreviewContrastUnderlay(strokeColor, backgroundColor)) {
        painter->setPen(previewPen(previewContrastUnderlayColor(strokeColor),
                                   strokeWidth + 2.2,
                                   strokeStyle,
                                   dashPattern));
        painter->drawPath(path);
    }
    painter->setPen(previewPen(strokeColor, strokeWidth, strokeStyle, dashPattern));
    painter->drawPath(path);
}

void drawPreviewLineStroke(QPainter *painter,
                           const QLineF &line,
                           const QColor &strokeColor,
                           qreal strokeWidth,
                           Qt::PenStyle strokeStyle,
                           const QVector<qreal> &dashPattern,
                           const QColor &backgroundColor)
{
    if (painter == nullptr) {
        return;
    }

    painter->setBrush(Qt::NoBrush);
    if (needsPreviewContrastUnderlay(strokeColor, backgroundColor)) {
        painter->setPen(previewPen(previewContrastUnderlayColor(strokeColor),
                                   strokeWidth + 2.2,
                                   strokeStyle,
                                   dashPattern));
        painter->drawLine(line);
    }
    painter->setPen(previewPen(strokeColor, strokeWidth, strokeStyle, dashPattern));
    painter->drawLine(line);
}

struct PreviewSample
{
    QPointF point;
    QPointF tangent;
    QPointF normal;
};

std::optional<PreviewSample> samplePathAt(const QPainterPath &path, qreal distance)
{
    const qreal length = path.length();
    if (length <= 0.001) {
        return std::nullopt;
    }

    const qreal clampedDistance = qBound<qreal>(0.0, distance, length);
    const qreal tangentStep = qBound<qreal>(0.5, length * 0.01, 4.0);
    const QPointF before = path.pointAtPercent(path.percentAtLength(qMax<qreal>(0.0, clampedDistance - tangentStep)));
    const QPointF after = path.pointAtPercent(path.percentAtLength(qMin<qreal>(length, clampedDistance + tangentStep)));
    QPointF tangent = after - before;
    const qreal tangentLength = std::hypot(tangent.x(), tangent.y());
    if (tangentLength <= 0.001) {
        return std::nullopt;
    }

    tangent /= tangentLength;
    QPointF normal(tangent.y(), -tangent.x());
    const qreal normalLength = std::hypot(normal.x(), normal.y());
    if (normalLength <= 0.001) {
        return std::nullopt;
    }
    normal /= normalLength;

    return PreviewSample{path.pointAtPercent(path.percentAtLength(clampedDistance)), tangent, normal};
}

qreal decorationSideFactor(MapEditorLineDecorationSide side)
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

qreal effectiveDecorationOffset(const MapEditorLineDecorationStyle &decoration,
                                int markerIndex,
                                int seed)
{
    qreal offset = decoration.side == MapEditorLineDecorationSide::Center
        ? decoration.offset
        : decorationSideFactor(decoration.side) * decoration.offset;
    if (decoration.offsetJitter > 0.0) {
        offset += stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x91f2U) * decoration.offsetJitter;
    }
    return offset;
}

void drawOffsetPreviewStroke(QPainter *painter,
                             const QPainterPath &path,
                             const MapEditorLineDecorationStyle &decoration,
                             const QColor &fallbackColor,
                             const QColor &backgroundColor,
                             qreal offset)
{
    QVector<QPointF> points;
    const qreal length = path.length();
    const int sampleCount = qBound(8, static_cast<int>(length / 8.0), 80);
    points.reserve(sampleCount);
    for (int index = 0; index < sampleCount; ++index) {
        const qreal distance = length * static_cast<qreal>(index) / static_cast<qreal>(sampleCount - 1);
        if (const std::optional<PreviewSample> sample = samplePathAt(path, distance)) {
            points.append(sample->point + sample->normal * offset);
        }
    }
    if (points.size() < 2) {
        return;
    }

    drawPreviewPolyline(painter,
                        QPolygonF(points),
                        decoration.strokeColor.value_or(fallbackColor),
                        decoration.strokeWidth,
                        decoration.strokeStyle,
                        decoration.dashPattern,
                        backgroundColor);
}

void drawPreviewSymbol(QPainter *painter,
                       const QPointF &center,
                       qreal size,
                       qreal angleDegrees,
                       MapEditorPointSymbol symbol,
                       const QColor &strokeColor,
                       const std::optional<QColor> &fillColor,
                       qreal strokeWidth,
                       const QColor &backgroundColor)
{
    const QRectF symbolRect(center.x() - size / 2.0,
                            center.y() - size / 2.0,
                            size,
                            size);
    QPainterPath symbolPath = mapEditorPointSymbolPath(symbol, symbolRect);
    if (std::abs(angleDegrees) > 0.001) {
        QTransform transform;
        transform.translate(center.x(), center.y());
        transform.rotate(angleDegrees);
        transform.translate(-center.x(), -center.y());
        symbolPath = transform.map(symbolPath);
    }

    if (needsPreviewContrastUnderlay(strokeColor, backgroundColor)) {
        painter->setBrush(Qt::NoBrush);
        painter->setPen(previewPen(previewContrastUnderlayColor(strokeColor),
                                   strokeWidth + 2.2,
                                   Qt::SolidLine));
        painter->drawPath(symbolPath);
    }
    if (mapEditorPointSymbolUsesFill(symbol) && fillColor.has_value()) {
        painter->setBrush(QBrush(fillColor.value()));
    } else {
        painter->setBrush(Qt::NoBrush);
    }
    painter->setPen(previewPen(strokeColor, strokeWidth, Qt::SolidLine));
    painter->drawPath(symbolPath);
}

void drawLineDecorationsPreview(QPainter *painter,
                                const QPainterPath &path,
                                const QVector<MapEditorLineDecorationStyle> &decorations,
                                const QColor &fallbackColor,
                                const QColor &backgroundColor)
{
    const qreal length = path.length();
    if (length <= 0.001) {
        return;
    }

    for (const MapEditorLineDecorationStyle &decoration : decorations) {
        switch (decoration.kind) {
        case MapEditorLineDecorationKind::OffsetStroke:
            drawOffsetPreviewStroke(painter, path, decoration, fallbackColor, backgroundColor, decoration.offset);
            break;
        case MapEditorLineDecorationKind::Parallel: {
            const QVector<qreal> offsets = decoration.offsets.isEmpty()
                ? QVector<qreal>{decoration.offset}
                : decoration.offsets;
            for (const qreal offset : offsets) {
                drawOffsetPreviewStroke(painter, path, decoration, fallbackColor, backgroundColor, offset);
            }
            break;
        }
        case MapEditorLineDecorationKind::Ticks:
        case MapEditorLineDecorationKind::Rungs:
        case MapEditorLineDecorationKind::Teeth:
        case MapEditorLineDecorationKind::Symbols: {
            const qreal spacing = qMax<qreal>(1.0, decoration.spacing);
            const int rawMarkerCount = static_cast<int>(std::floor(length / spacing));
            const int markerCount = rawMarkerCount > 0
                ? qMin(rawMarkerCount, 96)
                : 1;
            const int seed = decoration.seed.value_or(1);
            const QColor decorationStrokeColor = decoration.strokeColor.value_or(fallbackColor);
            painter->setPen(previewPen(decorationStrokeColor,
                                       decoration.strokeWidth,
                                       decoration.strokeStyle,
                                       decoration.dashPattern));
            for (int markerIndex = 0; markerIndex < markerCount; ++markerIndex) {
                const qreal distance = rawMarkerCount > markerCount
                    ? (markerIndex + 0.5) * length / static_cast<qreal>(markerCount)
                    : (rawMarkerCount > 0
                        ? (markerIndex + 0.5) * spacing
                        : length * 0.5);
                const std::optional<PreviewSample> sample = samplePathAt(path, distance);
                if (!sample.has_value()) {
                    continue;
                }

                const QPointF center = sample->point + sample->normal * effectiveDecorationOffset(decoration, markerIndex, seed);
                if (decoration.kind == MapEditorLineDecorationKind::Ticks) {
                    const qreal sideFactor = decoration.side == MapEditorLineDecorationSide::Center
                        ? 0.0
                        : decorationSideFactor(decoration.side);
                    if (decoration.side == MapEditorLineDecorationSide::Center) {
                        const QPointF half = sample->normal * (decoration.length / 2.0);
                        drawPreviewLineStroke(painter,
                                              QLineF(center - half, center + half),
                                              decorationStrokeColor,
                                              decoration.strokeWidth,
                                              decoration.strokeStyle,
                                              decoration.dashPattern,
                                              backgroundColor);
                    } else {
                        drawPreviewLineStroke(painter,
                                              QLineF(center, center + sample->normal * sideFactor * decoration.length),
                                              decorationStrokeColor,
                                              decoration.strokeWidth,
                                              decoration.strokeStyle,
                                              decoration.dashPattern,
                                              backgroundColor);
                    }
                } else if (decoration.kind == MapEditorLineDecorationKind::Rungs) {
                    const qreal fromOffset = decoration.fromOffset.value_or(-decoration.length / 2.0);
                    const qreal toOffset = decoration.toOffset.value_or(decoration.length / 2.0);
                    drawPreviewLineStroke(painter,
                                          QLineF(sample->point + sample->normal * fromOffset,
                                                 sample->point + sample->normal * toOffset),
                                          decorationStrokeColor,
                                          decoration.strokeWidth,
                                          decoration.strokeStyle,
                                          decoration.dashPattern,
                                          backgroundColor);
                } else if (decoration.kind == MapEditorLineDecorationKind::Teeth) {
                    const qreal sideFactor = decoration.side == MapEditorLineDecorationSide::Left ? -1.0 : 1.0;
                    const qreal halfBase = decoration.size * 0.45;
                    QPainterPath tooth;
                    tooth.moveTo(center - sample->tangent * halfBase);
                    tooth.lineTo(center + sample->normal * sideFactor * decoration.size);
                    tooth.lineTo(center + sample->tangent * halfBase);
                    tooth.closeSubpath();
                    const QColor toothFill = decoration.fillColor.value_or(decoration.strokeColor.value_or(fallbackColor));
                    if (needsPreviewContrastUnderlay(toothFill, backgroundColor)) {
                        painter->setBrush(Qt::NoBrush);
                        painter->setPen(previewPen(previewContrastUnderlayColor(toothFill),
                                                   decoration.strokeWidth + 2.2,
                                                   decoration.strokeStyle,
                                                   decoration.dashPattern));
                        painter->drawPath(tooth);
                    }
                    painter->setBrush(QBrush(toothFill));
                    painter->setPen(previewPen(decorationStrokeColor,
                                               decoration.strokeWidth,
                                               decoration.strokeStyle,
                                               decoration.dashPattern));
                    painter->drawPath(tooth);
                    painter->setBrush(Qt::NoBrush);
                } else {
                    const qreal tangentAngle = std::atan2(sample->tangent.y(), sample->tangent.x()) * 180.0 / 3.14159265358979323846;
                    const qreal sizeJitter = decoration.sizeJitter <= 0.0
                        ? 0.0
                        : stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0x53acU) * decoration.sizeJitter;
                    const qreal angleJitter = decoration.angleJitter <= 0.0
                        ? 0.0
                        : stableRandomSigned(static_cast<quint32>(seed), markerIndex, 0, 0xa174U) * decoration.angleJitter;
                    drawPreviewSymbol(painter,
                                      center,
                                      qBound<qreal>(4.0, decoration.size + sizeJitter, 22.0),
                                      tangentAngle + decoration.angle + angleJitter,
                                      decoration.symbol,
                                      decorationStrokeColor,
                                      decoration.fillColor.has_value()
                                          ? std::optional<QColor>(decoration.fillColor.value())
                                          : std::nullopt,
                                      decoration.strokeWidth,
                                      backgroundColor);
                }
            }
            break;
        }
        }
    }
}

void drawAreaPatternPreview(QPainter *painter,
                            const QPainterPath &path,
                            const MapEditorAreaFillPatternStyle &pattern,
                            const QColor &fallbackColor,
                            const QColor &backgroundColor)
{
    painter->save();
    painter->setClipPath(path, Qt::IntersectClip);
    const QRectF bounds = path.boundingRect().adjusted(-8.0, -8.0, 8.0, 8.0);
    if (pattern.kind == MapEditorFillPatternKind::Hatch
        || pattern.kind == MapEditorFillPatternKind::CrossHatch) {
        const auto drawHatch = [&](qreal angleDegrees) {
            const qreal spacing = qBound<qreal>(7.0, pattern.spacing, 18.0);
            const qreal radians = angleDegrees * 3.14159265358979323846 / 180.0;
            const QPointF direction(std::cos(radians), std::sin(radians));
            const QPointF normal(-direction.y(), direction.x());
            const QPointF center = bounds.center();
            const qreal extent = std::hypot(bounds.width(), bounds.height()) * 1.2;
            painter->setPen(previewPen(readablePreviewColor(pattern.strokeColor.value_or(fallbackColor),
                                                            backgroundColor,
                                                            fallbackColor),
                                       pattern.strokeWidth,
                                       pattern.strokeStyle,
                                       pattern.dashPattern));
            for (qreal offset = -extent; offset <= extent; offset += spacing) {
                const QPointF lineCenter = center + normal * offset;
                painter->drawLine(QLineF(lineCenter - direction * extent, lineCenter + direction * extent));
            }
        };
        drawHatch(pattern.angle);
        if (pattern.kind == MapEditorFillPatternKind::CrossHatch) {
            drawHatch(pattern.angle + 90.0);
        }
    } else if (pattern.kind == MapEditorFillPatternKind::Dots) {
        const QColor dotColor = readablePreviewColor(pattern.dotColor.value_or(pattern.strokeColor.value_or(fallbackColor)),
                                                     backgroundColor,
                                                     fallbackColor);
        const qreal spacing = qBound<qreal>(5.0, pattern.spacing, 22.0);
        const qreal symbolSize = qBound<qreal>(1.8, pattern.size, 12.0);
        const qreal sizeJitter = qBound<qreal>(0.0, pattern.sizeJitter, 12.0);
        const qreal offsetJitter = qBound<qreal>(0.0, pattern.offsetJitter, spacing * 0.48);
        const int seedValue = pattern.seed.value_or(1);
        const quint32 seed = static_cast<quint32>(seedValue == 0 ? 1 : seedValue);
        const qreal startX = (std::floor(bounds.left() / spacing) * spacing) - spacing;
        const qreal startY = (std::floor(bounds.top() / spacing) * spacing) - spacing;
        const int cols = qBound(1, static_cast<int>(std::ceil(bounds.width() / spacing)) + 4, 160);
        const int rows = qBound(1, static_cast<int>(std::ceil(bounds.height() / spacing)) + 4, 160);
        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const int index = row * 4096 + col;
                const qreal jitterX = offsetJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, 2, index, 0x14f21U) * offsetJitter;
                const qreal jitterY = offsetJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, 3, index, 0x6a223U) * offsetJitter;
                const qreal symbolSizeJitter = sizeJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, 5, index, 0x3c6efU) * sizeJitter;
                const qreal angleJitter = pattern.angleJitter <= 0.0
                    ? 0.0
                    : stableRandomSigned(seed, 4, index, 0x92d41U) * pattern.angleJitter;
                const qreal currentSymbolSize = qBound<qreal>(1.4, symbolSize + symbolSizeJitter, 14.0);
                drawPreviewSymbol(painter,
                                  QPointF(startX + (static_cast<qreal>(col) * spacing) + jitterX,
                                          startY + (static_cast<qreal>(row) * spacing) + jitterY),
                                  currentSymbolSize,
                                  angleJitter,
                                  pattern.symbol,
                                  dotColor,
                                  mapEditorPointSymbolUsesFill(pattern.symbol) ? std::optional<QColor>(dotColor) : std::nullopt,
                                  qMax<qreal>(0.7, currentSymbolSize * 0.18),
                                  backgroundColor);
            }
        }
    }
    painter->restore();
}
}

MapEditorStylePreviewWidget::MapEditorStylePreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(82);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MapEditorStylePreviewWidget::setStyleSelection(const QString &commandKind,
                                                    const QString &rawType,
                                                    const QString &subtype)
{
    const QString normalizedCommand = commandKind.trimmed().toLower();
    const bool shouldBeVisible = normalizedCommand == QStringLiteral("point")
        || normalizedCommand == QStringLiteral("line")
        || normalizedCommand == QStringLiteral("area");
    setVisible(shouldBeVisible);
    if (commandKind_ == normalizedCommand
        && rawType_ == rawType.trimmed()
        && subtype_ == subtype.trimmed()) {
        return;
    }

    commandKind_ = normalizedCommand;
    rawType_ = rawType.trimmed();
    subtype_ = subtype.trimmed();
    update();
}

void MapEditorStylePreviewWidget::clearStyleSelection()
{
    commandKind_.clear();
    rawType_.clear();
    subtype_.clear();
    setVisible(false);
    update();
}

QSize MapEditorStylePreviewWidget::minimumSizeHint() const
{
    return QSize(150, 82);
}

QSize MapEditorStylePreviewWidget::sizeHint() const
{
    return QSize(260, 88);
}

void MapEditorStylePreviewWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QPalette pal = palette();
    const QColor frameColor = pal.color(QPalette::Mid);
    const QColor fillColor = previewTileBackgroundColor(pal);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(previewPen(frameColor, 1.0, Qt::SolidLine));
    painter.setBrush(fillColor);
    painter.drawRoundedRect(frame, 5.0, 5.0);

    const QRectF content = frame.adjusted(12.0, 8.0, -12.0, -8.0);
    if (commandKind_.isEmpty() || rawType_.isEmpty()) {
        painter.setPen(readablePreviewColor(pal.color(QPalette::Disabled, QPalette::Text),
                                            fillColor,
                                            QColor(96, 96, 96)));
        painter.drawText(content, Qt::AlignCenter, tr("No style preview"));
        return;
    }

    const MapEditorObjectStyleCatalog catalog = mapEditorObjectStyleCatalog();
    const QColor previewBackground = fillColor;
    const QColor fallbackStroke = readablePreviewColor(pal.color(QPalette::Text),
                                                       previewBackground,
                                                       QColor(28, 28, 30));
    const QColor fallbackAccent = readablePreviewColor(pal.color(QPalette::Highlight),
                                                       previewBackground,
                                                       QColor(38, 114, 211));

    if (commandKind_ == QStringLiteral("point")) {
        const MapEditorResolvedPointStyle style = resolveMapEditorPointStyle(catalog, rawType_, subtype_);
        const qreal symbolSize = qBound<qreal>(14.0, style.size * 1.7, qMin(content.width(), content.height()) - 4.0);
        std::optional<QColor> pointFill = style.fillColor;
        if (!pointFill.has_value()) {
            pointFill = fallbackAccent;
        } else {
            pointFill = readablePreviewColor(pointFill.value(), previewBackground, fallbackAccent);
        }
        const QColor pointStroke = readablePreviewColor(style.strokeColor.value_or(fallbackStroke),
                                                        previewBackground,
                                                        fallbackStroke);
        drawPreviewSymbol(&painter,
                          content.center(),
                          symbolSize,
                          0.0,
                          style.symbol,
                          pointStroke,
                          pointFill,
                          qBound<qreal>(0.7, style.outlineWidth, 4.0),
                          previewBackground);
        return;
    }

    if (commandKind_ == QStringLiteral("line")) {
        const MapEditorResolvedLineStyle style = resolveMapEditorLineStyle(catalog, rawType_, subtype_);
        const QColor strokeColor = style.strokeColor.value_or(fallbackStroke);
        QPainterPath path;
        path.moveTo(content.left(), content.center().y() + content.height() * 0.22);
        path.cubicTo(content.left() + content.width() * 0.28,
                     content.top() - 2.0,
                     content.left() + content.width() * 0.62,
                     content.bottom() + 2.0,
                     content.right(),
                     content.center().y() - content.height() * 0.18);
        if (style.strokeVisible) {
            drawPreviewPathStroke(&painter,
                                  path,
                                  strokeColor,
                                  qBound<qreal>(1.0, style.strokeWidth, 8.0),
                                  style.penStyle,
                                  style.dashPattern,
                                  previewBackground);
        }
        drawLineDecorationsPreview(&painter, path, style.decorations, strokeColor, previewBackground);
        return;
    }

    if (commandKind_ == QStringLiteral("area")) {
        const MapEditorResolvedAreaStyle style = resolveMapEditorAreaStyle(catalog, rawType_, subtype_);
        const QColor strokeColor = readablePreviewColor(style.strokeColor.value_or(fallbackStroke),
                                                        previewBackground,
                                                        fallbackStroke);
        QColor areaFill = style.fillColor.value_or(fallbackAccent);
        areaFill.setAlphaF(qBound(0.04, style.fillOpacity, 0.45));
        QPainterPath path;
        path.addRoundedRect(frame.adjusted(4.0, 4.0, -4.0, -4.0), 5.0, 5.0);

        painter.setBrush(areaFill);
        painter.setPen(previewPen(strokeColor,
                                  qBound<qreal>(1.0, style.strokeWidth, 6.0),
                                  style.penStyle,
                                  style.dashPattern));
        painter.drawPath(path);
        if (style.fillPattern.has_value()) {
            QColor patternColor = strokeColor;
            patternColor.setAlpha(180);
            drawAreaPatternPreview(&painter, path, style.fillPattern.value(), patternColor, previewBackground);
        }
    }
}
}
