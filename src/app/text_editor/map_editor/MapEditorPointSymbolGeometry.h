#pragma once

#include "MapEditorObjectStyleCatalog.h"

#include <QPainterPath>
#include <QPolygonF>
#include <QPointF>
#include <QRectF>
#include <QTransform>
#include <QtGlobal>

#include <cmath>

namespace TherionStudio
{
inline bool mapEditorPointSymbolUsesFill(MapEditorPointSymbol symbol)
{
    switch (symbol) {
    case MapEditorPointSymbol::Asterisk:
    case MapEditorPointSymbol::Plus:
    case MapEditorPointSymbol::X:
        return false;
    case MapEditorPointSymbol::Circle:
    case MapEditorPointSymbol::Oval:
    case MapEditorPointSymbol::Square:
    case MapEditorPointSymbol::Diamond:
    case MapEditorPointSymbol::Triangle:
    case MapEditorPointSymbol::Star:
        return true;
    }
    return true;
}

inline qreal mapEditorPointSymbolDegreesToRadians(qreal degrees)
{
    constexpr qreal kPi = 3.141592653589793238462643383279502884;
    return degrees * kPi / 180.0;
}

inline void mapEditorPointSymbolAddCenteredLine(QPainterPath *path,
                                                const QPointF &center,
                                                qreal radius,
                                                qreal angleDegrees)
{
    if (path == nullptr) {
        return;
    }

    const qreal angleRadians = mapEditorPointSymbolDegreesToRadians(angleDegrees);
    const QPointF delta(std::cos(angleRadians) * radius, std::sin(angleRadians) * radius);
    path->moveTo(center - delta);
    path->lineTo(center + delta);
}

inline QPainterPath mapEditorPointSymbolPath(MapEditorPointSymbol symbol, const QRectF &rect)
{
    QPainterPath path;
    const QPointF center = rect.center();
    const qreal halfWidth = rect.width() / 2.0;
    const qreal halfHeight = rect.height() / 2.0;

    switch (symbol) {
    case MapEditorPointSymbol::Circle:
        path.addEllipse(rect);
        return path;
    case MapEditorPointSymbol::Oval: {
        QRectF ovalRect = rect;
        ovalRect.setHeight(rect.height() * 0.55);
        ovalRect.moveCenter(rect.center());
        path.addEllipse(ovalRect);
        return path;
    }
    case MapEditorPointSymbol::Square:
        path.addRect(rect);
        return path;
    case MapEditorPointSymbol::Diamond:
        path.addPolygon(QPolygonF({
            QPointF(center.x(), rect.top()),
            QPointF(rect.right(), center.y()),
            QPointF(center.x(), rect.bottom()),
            QPointF(rect.left(), center.y())
        }));
        path.closeSubpath();
        return path;
    case MapEditorPointSymbol::Triangle:
        path.addPolygon(QPolygonF({
            QPointF(center.x(), rect.top()),
            QPointF(rect.right(), rect.bottom()),
            QPointF(rect.left(), rect.bottom())
        }));
        path.closeSubpath();
        return path;
    case MapEditorPointSymbol::Star: {
        QPolygonF starPoints;
        starPoints.reserve(10);
        const qreal outerRadius = qMin(halfWidth, halfHeight);
        const qreal innerRadius = outerRadius * 0.45;
        for (int index = 0; index < 10; ++index) {
            const qreal radius = (index % 2 == 0) ? outerRadius : innerRadius;
            const qreal angleRadians = mapEditorPointSymbolDegreesToRadians(-90.0 + index * 36.0);
            starPoints.append(QPointF(center.x() + std::cos(angleRadians) * radius,
                                      center.y() + std::sin(angleRadians) * radius));
        }
        path.addPolygon(starPoints);
        path.closeSubpath();
        return path;
    }
    case MapEditorPointSymbol::Asterisk:
        mapEditorPointSymbolAddCenteredLine(&path, center, qMin(halfWidth, halfHeight), 0.0);
        mapEditorPointSymbolAddCenteredLine(&path, center, qMin(halfWidth, halfHeight), 60.0);
        mapEditorPointSymbolAddCenteredLine(&path, center, qMin(halfWidth, halfHeight), 120.0);
        return path;
    case MapEditorPointSymbol::Plus:
        path.moveTo(QPointF(rect.left(), center.y()));
        path.lineTo(QPointF(rect.right(), center.y()));
        path.moveTo(QPointF(center.x(), rect.top()));
        path.lineTo(QPointF(center.x(), rect.bottom()));
        return path;
    case MapEditorPointSymbol::X:
        path.moveTo(rect.topLeft());
        path.lineTo(rect.bottomRight());
        path.moveTo(rect.topRight());
        path.lineTo(rect.bottomLeft());
        return path;
    }

    path.addEllipse(rect);
    return path;
}

inline QPainterPath mapEditorPointSymbolPartPath(const MapEditorPointSymbolPart &part,
                                                 const QRectF &baseRect,
                                                 qreal baseStyleSize)
{
    const qreal scale = qMin(baseRect.width(), baseRect.height()) / qMax<qreal>(0.001, baseStyleSize);
    const auto mapPoint = [&](const QPointF &point) {
        return baseRect.center() + QPointF(point.x() * scale, point.y() * scale);
    };

    QPainterPath path;
    QPointF transformCenter = mapPoint(QPointF(part.x, part.y));
    switch (part.kind) {
    case MapEditorSymbolPartKind::Symbol: {
        const qreal symbolSize = qMax<qreal>(0.001, part.size * scale);
        const QRectF symbolRect(transformCenter.x() - (symbolSize / 2.0),
                                transformCenter.y() - (symbolSize / 2.0),
                                symbolSize,
                                symbolSize);
        path = mapEditorPointSymbolPath(part.symbol, symbolRect);
        break;
    }
    case MapEditorSymbolPartKind::Line:
        path.moveTo(mapPoint(QPointF(part.x1, part.y1)));
        path.lineTo(mapPoint(QPointF(part.x2, part.y2)));
        transformCenter = mapPoint(QPointF((part.x1 + part.x2) / 2.0, (part.y1 + part.y2) / 2.0));
        break;
    case MapEditorSymbolPartKind::Polyline:
        if (!part.points.isEmpty()) {
            QPolygonF polyline;
            polyline.reserve(part.points.size());
            for (const QPointF &point : part.points) {
                polyline.append(mapPoint(point));
            }
            transformCenter = polyline.boundingRect().center();
            path.moveTo(mapPoint(part.points.first()));
            for (int index = 1; index < part.points.size(); ++index) {
                path.lineTo(mapPoint(part.points.at(index)));
            }
        }
        break;
    case MapEditorSymbolPartKind::Polygon: {
        QPolygonF polygon;
        polygon.reserve(part.points.size());
        for (const QPointF &point : part.points) {
            polygon.append(mapPoint(point));
        }
        path.addPolygon(polygon);
        path.closeSubpath();
        if (!polygon.isEmpty()) {
            transformCenter = polygon.boundingRect().center();
        }
        break;
    }
    case MapEditorSymbolPartKind::Ellipse: {
        const qreal width = qMax<qreal>(0.001, part.width * scale);
        const qreal height = qMax<qreal>(0.001, part.height * scale);
        const QRectF ellipseRect(transformCenter.x() - (width / 2.0),
                                 transformCenter.y() - (height / 2.0),
                                 width,
                                 height);
        path.addEllipse(ellipseRect);
        break;
    }
    }

    if (std::abs(part.angle) > 0.001 && !path.isEmpty()) {
        QTransform transform;
        transform.translate(transformCenter.x(), transformCenter.y());
        transform.rotate(part.angle);
        transform.translate(-transformCenter.x(), -transformCenter.y());
        path = transform.map(path);
    }
    return path;
}

inline bool mapEditorSymbolPartUsesFill(const MapEditorPointSymbolPart &part)
{
    switch (part.kind) {
    case MapEditorSymbolPartKind::Symbol:
        return mapEditorPointSymbolUsesFill(part.symbol);
    case MapEditorSymbolPartKind::Polygon:
    case MapEditorSymbolPartKind::Ellipse:
        return part.fill;
    case MapEditorSymbolPartKind::Line:
    case MapEditorSymbolPartKind::Polyline:
        return false;
    }
    return false;
}
} // namespace TherionStudio
