#pragma once

#include "MapEditorObjectStyleCatalog.h"

#include <QColor>
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QVector>

namespace TherionStudio
{
class MapEditorLineDecorationItem final : public QGraphicsPathItem
{
public:
    MapEditorLineDecorationItem(const QPainterPath &path,
                                const QVector<MapEditorLineDecorationStyle> &decorations,
                                const QColor &fallbackColor,
                                bool reversed,
                                int fallbackSeed,
                                QGraphicsItem *parent = nullptr);

    void setDecorationPath(const QPainterPath &path);
    QRectF boundingRect() const override;

protected:
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    enum class DrawMode
    {
        Ticks,
        Rungs,
        Teeth,
        Symbols
    };

    void updatePaintBounds();
    void drawOffsetStroke(QPainter *painter,
                          const MapEditorLineDecorationStyle &decoration,
                          qreal offset,
                          qreal zoomOutScale);
    void drawRepeated(QPainter *painter,
                      const MapEditorLineDecorationStyle &decoration,
                      qreal zoomOutScale,
                      DrawMode mode);
    void drawTick(QPainter *painter,
                  const MapEditorLineDecorationStyle &decoration,
                  const QPointF &point,
                  const QPointF &normal,
                  int markerIndex);
    void drawRung(QPainter *painter,
                  const MapEditorLineDecorationStyle &decoration,
                  const QPointF &point,
                  const QPointF &normal);
    void drawTooth(QPainter *painter,
                   const MapEditorLineDecorationStyle &decoration,
                   const QPointF &point,
                   const QPointF &tangent,
                   const QPointF &normal,
                   int markerIndex);
    void drawSymbol(QPainter *painter,
                    const MapEditorLineDecorationStyle &decoration,
                    const QPointF &point,
                    const QPointF &tangent,
                    const QPointF &normal,
                    int markerIndex,
                    qreal zoomOutScale);

    QVector<MapEditorLineDecorationStyle> decorations_;
    QColor fallbackColor_;
    bool reversed_ = false;
    int fallbackSeed_ = 0;
    QRectF paintBounds_;
};
}
