#pragma once

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"
#include "BlockEditorMovePlanner.h"
#include "BlockEditorSourceController.h"

#include <functional>
#include <QPointF>

class QWidget;

namespace TherionStudio
{
struct BlockEditorMoveContext
{
    QWidget *dialogParent = nullptr;
    std::function<BlockEditorSourceContext()> sourceContext;
    BlockEditorDocumentOutlineContext documentOutlineContext;
    BlockEditorDropTargetContext dropTargetContext;
    BlockEditorMovePlannerContext movePlannerContext;
    std::function<void()> clearBlockMovePreview;
    std::function<bool()> isBlocksModeSupportedForCurrentFile;
    std::function<int(const QPointF &)> resolveEndHintContainerStartLineAtScenePos;
    std::function<bool(const QString &, const QString &)> isCompatibleChildKindForBlocks;
    std::function<QString(const char *)> translate;
};

class BlockEditorMoveController final
{
public:
    explicit BlockEditorMoveController(BlockEditorMoveContext context);

    void moveBlock(int lineNumber, const QPointF &scenePos);

private:
    QString tr(const char *text) const;

    BlockEditorMoveContext context_;
};
}
