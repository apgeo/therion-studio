#pragma once

#include <QHash>
#include <QPointF>
#include <QVector>

#include <functional>

class QGraphicsItem;
class QGraphicsScene;

namespace TherionStudio
{
struct BlockEditorDocumentEntry;

struct BlockEditorDropTargetContext
{
    QGraphicsScene *scene = nullptr;
    std::function<int(const QGraphicsItem *)> blockCanvasItemLineNumber;
    std::function<QGraphicsItem *(QGraphicsItem *)> resolveBlockCanvasItem;
};

struct BlockEditorSceneVerticalPlacement
{
    bool beforeAllBlocks = false;
    bool afterAllBlocks = false;
};

class BlockEditorDropTargetResolver final
{
public:
    using SceneItemsByLine = QHash<int, QGraphicsItem *>;
    using EntryPredicate = std::function<bool(const BlockEditorDocumentEntry &)>;

    explicit BlockEditorDropTargetResolver(BlockEditorDropTargetContext context);

    SceneItemsByLine collectSceneItemsByLine() const;
    BlockEditorSceneVerticalPlacement resolveVerticalPlacement(const QVector<BlockEditorDocumentEntry> &entries,
                                                               const SceneItemsByLine &sceneItemsByLine,
                                                               const QPointF &scenePos) const;
    QGraphicsItem *resolveTargetItem(const QVector<BlockEditorDocumentEntry> &entries,
                                     const SceneItemsByLine &sceneItemsByLine,
                                     const QPointF &scenePos,
                                     int directExcludedLineNumber = 0,
                                     const EntryPredicate &excludeEntry = EntryPredicate()) const;

private:
    BlockEditorDropTargetContext context_;
};
}
