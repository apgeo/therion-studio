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
struct BlockEditorInsertionPlan
{
    int insertBeforeLine = 1;
    int parentLine = 0;
    QString parentKind;
    bool compatible = true;
};

struct BlockEditorInsertionPlannerContext
{
    std::function<int(const QGraphicsItem *)> blockCanvasItemLineNumber;
    std::function<bool(const QString &, const QString &)> isCompatibleChildKindForBlocks;
};

class BlockEditorInsertionPlanner final
{
public:
    explicit BlockEditorInsertionPlanner(BlockEditorInsertionPlannerContext context);

    BlockEditorInsertionPlan planInsertion(const QString &normalizedKind,
                                           const QVector<BlockEditorDocumentEntry> &entries,
                                           const QHash<int, int> &entryIndexByStartLine,
                                           const BlockEditorSceneVerticalPlacement &verticalPlacement,
                                           int explicitEndHintContainerLine,
                                           QGraphicsItem *targetBlockItem,
                                           const QPointF &scenePos,
                                           int appendLineNumber) const;

private:
    BlockEditorInsertionPlannerContext context_;
};
}
