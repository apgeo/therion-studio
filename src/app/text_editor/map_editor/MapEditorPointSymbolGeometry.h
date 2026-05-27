#pragma once

#include "MapEditorObjectStyleCatalog.h"

#include <QPainterPath>
#include <QPolygonF>
#include <QPointF>
#include <QRectF>
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
} // namespace TherionStudio
