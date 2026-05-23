#include "TextEditorTab.h"

#include "block_editor/BlockEditorCanvasBoundaryController.h"
#include "block_editor/BlockEditorCanvasRebuildController.h"

namespace TherionStudio
{
BlockEditorCanvasBoundaryContext TextEditorTab::blockEditorCanvasBoundaryContext() const
{
    BlockEditorCanvasBoundaryContext context;
    context.scene = blockCanvasScene_;
    context.containerBoundaryGuideItems = &blockContainerBoundaryGuideItems_;
    return context;
}

BlockEditorCanvasRebuildContext TextEditorTab::blockEditorCanvasRebuildContext()
{
    BlockEditorCanvasRebuildContext context;
    context.scene = blockCanvasScene_;
    context.movePreviewLine = &blockMovePreviewLine_;
    context.containerBoundaryGuideItems = &blockContainerBoundaryGuideItems_;
    context.containerBoundaryEndYByLine = &blockContainerBoundaryEndYByLine_;
    context.selectedLineNumber = &blockDetailsSelectedLineNumber_;
    context.tearingDown = &tearingDown_;
    context.blocksModeActive = &blocksModeActive_;
    context.commandMetadata = &commandMetadata_;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.isBlocksModeSupportedForCurrentFile = [this]() {
        return isBlocksModeSupportedForCurrentFile();
    };
    context.ensureEncodingRootDirectiveForBlocks = [this]() {
        return ensureEncodingRootDirectiveForBlocks();
    };
    context.handleBlockDeleteRequest = [this](int lineNumber) {
        handleBlockDeleteRequest(lineNumber);
    };
    context.handleBlockMoveRequest = [this](int lineNumber, const QPointF &scenePos) {
        handleBlockMoveRequest(lineNumber, scenePos);
    };
    context.updateBlockMovePreview = [this](int sourceLineNumber, const QPointF &scenePos) {
        updateBlockMovePreview(sourceLineNumber, scenePos);
    };
    context.clearBlockMovePreview = [this]() {
        clearBlockMovePreview();
    };
    context.refreshBlockDetailsSelectionFromScene = [this]() {
        refreshBlockDetailsSelectionFromScene();
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}
}
