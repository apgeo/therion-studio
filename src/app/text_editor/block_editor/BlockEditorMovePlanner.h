#pragma once

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>

#include <functional>

class QGraphicsItem;

namespace TherionStudio
{
struct BlockEditorMovePlan
{
    int destinationParentLine = 0;
    int insertBeforeLineOriginal = 1;
    QString destinationParentKind;
    bool resolved = false;
};

struct BlockEditorMovePlannerContext
{
    BlockEditorDropTargetContext dropTargetContext;
    std::function<int(const QGraphicsItem *)> blockCanvasItemLineNumber;
    std::function<bool(const QString &, const QString &)> isCompatibleChildKindForBlocks;
};

class BlockEditorMovePlanner final
{
public:
    explicit BlockEditorMovePlanner(BlockEditorMovePlannerContext context);

    BlockEditorMovePlan planMove(const BlockEditorDocumentEntry &sourceEntry,
                                 const QVector<BlockEditorDocumentEntry> &entries,
                                 const QHash<int, int> &entryIndexByStartLine,
                                 const BlockEditorDropTargetResolver::SceneItemsByLine &sceneItemsByLine,
                                 const BlockEditorSceneVerticalPlacement &verticalPlacement,
                                 int explicitEndHintContainerLine,
                                 const QPointF &scenePos,
                                 int appendLineNumber) const;

private:
    BlockEditorMovePlannerContext context_;
};
}
