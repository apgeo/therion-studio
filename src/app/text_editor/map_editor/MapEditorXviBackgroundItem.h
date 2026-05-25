#pragma once

#include <QColor>
#include <QGraphicsPixmapItem>
#include <QPainterPath>
#include <QRectF>
#include <QVector>

namespace TherionStudio
{
struct MapEditorXviSketchPathData
{
    QPainterPath path;
    QColor color;
    Qt::PenStyle style = Qt::SolidLine;
};

struct MapEditorXviLayerGeometryData
{
    QPainterPath gridPath;
    QPainterPath traverseShotPath;
    QPainterPath splayShotPath;
    QVector<MapEditorXviSketchPathData> sketchPaths;
    QRectF contentBounds;

    bool hasContent() const
    {
        bool hasSketch = false;
        for (const MapEditorXviSketchPathData &sketch : sketchPaths) {
            if (sketch.path.elementCount() > 0) {
                hasSketch = true;
                break;
            }
        }
        return !contentBounds.isEmpty()
            && (gridPath.elementCount() > 0
                || traverseShotPath.elementCount() > 0
                || splayShotPath.elementCount() > 0
                || hasSketch);
    }
};

class MapEditorXviBackgroundItem final : public QGraphicsPixmapItem
{
public:
    explicit MapEditorXviBackgroundItem(QGraphicsItem *parent = nullptr);

    void setGeometryData(const MapEditorXviLayerGeometryData &geometry);
    const MapEditorXviLayerGeometryData &geometryData() const;

    QRectF boundingRect() const override;
    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

private:
    MapEditorXviLayerGeometryData geometry_;
    QRectF paintBounds_;
};
}
