#pragma once

#include <functional>

#include <QHash>
#include <QPointF>

class QGraphicsItem;
class QGraphicsLineItem;
class QGraphicsScene;

namespace TherionStudio
{
struct BlockEditorMovePreviewContext
{
    QGraphicsScene *scene = nullptr;
    QGraphicsLineItem **previewLine = nullptr;
    QHash<int, qreal> *containerBoundaryEndYByLine = nullptr;
    std::function<QGraphicsItem *(QGraphicsItem *)> resolveBlockCanvasItem;
    std::function<int(const QGraphicsItem *)> blockCanvasItemLineNumber;
    std::function<int(const QPointF &)> resolveEndHintContainerStartLineAtScenePos;
};

class BlockEditorMovePreviewController final
{
public:
    explicit BlockEditorMovePreviewController(BlockEditorMovePreviewContext context);

    void updateMovePreview(int sourceLineNumber, const QPointF &scenePos);
    void clearMovePreview();

private:
    bool hasRequiredContext() const;
    void showPreviewLine(qreal y);

    BlockEditorMovePreviewContext context_;
};
}
