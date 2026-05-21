#pragma once

#include <QHash>
#include <QPointF>
#include <QVector>

#include <functional>

class QGraphicsItem;

namespace TherionStudio
{
class TextEditorTab;
struct BlockEditorDocumentEntry;

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

    explicit BlockEditorDropTargetResolver(const TextEditorTab *owner);

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
    const TextEditorTab *owner_ = nullptr;
};
}
