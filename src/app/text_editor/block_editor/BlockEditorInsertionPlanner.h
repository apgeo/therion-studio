#pragma once

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>

class QGraphicsItem;

namespace TherionStudio
{
class TextEditorTab;

struct BlockEditorInsertionPlan
{
    int insertBeforeLine = 1;
    int parentLine = 0;
    QString parentKind;
    bool compatible = true;
};

class BlockEditorInsertionPlanner final
{
public:
    explicit BlockEditorInsertionPlanner(const TextEditorTab *owner);

    BlockEditorInsertionPlan planInsertion(const QString &normalizedKind,
                                           const QVector<BlockEditorDocumentEntry> &entries,
                                           const QHash<int, int> &entryIndexByStartLine,
                                           const BlockEditorSceneVerticalPlacement &verticalPlacement,
                                           int explicitEndHintContainerLine,
                                           QGraphicsItem *targetBlockItem,
                                           const QPointF &scenePos,
                                           int appendLineNumber) const;

private:
    const TextEditorTab *owner_ = nullptr;
};
}
