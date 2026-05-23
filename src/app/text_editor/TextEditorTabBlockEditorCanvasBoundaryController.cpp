#include "TextEditorTab.h"

#include "block_editor/BlockEditorCanvasBoundaryController.h"

namespace TherionStudio
{
BlockEditorCanvasBoundaryContext TextEditorTab::blockEditorCanvasBoundaryContext() const
{
    BlockEditorCanvasBoundaryContext context;
    context.scene = blockCanvasScene_;
    context.containerBoundaryGuideItems = &blockContainerBoundaryGuideItems_;
    return context;
}
}
