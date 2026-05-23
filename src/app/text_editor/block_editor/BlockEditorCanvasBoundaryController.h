#pragma once

#include <QList>
#include <QPointF>

class QGraphicsLineItem;
class QGraphicsScene;

namespace TherionStudio
{
struct BlockEditorCanvasBoundaryContext
{
    QGraphicsScene *scene = nullptr;
    const QList<QGraphicsLineItem *> *containerBoundaryGuideItems = nullptr;
};

class BlockEditorCanvasBoundaryController final
{
public:
    explicit BlockEditorCanvasBoundaryController(BlockEditorCanvasBoundaryContext context);

    int resolveEndHintContainerStartLineAtScenePos(const QPointF &scenePos) const;

private:
    BlockEditorCanvasBoundaryContext context_;
};
}
