#include "TextEditorTab.h"

#include "block_editor/BlockEditorDocumentOutlineBuilder.h"
#include "block_editor/BlockEditorDropTargetResolver.h"
#include "block_editor/BlockEditorInsertionController.h"
#include "block_editor/BlockEditorInsertionPlanner.h"
#include "block_editor/BlockEditorMoveController.h"
#include "block_editor/BlockEditorMovePlanner.h"
#include "block_editor/BlockEditorMovePreviewController.h"

#include <memory>

#include <QPlainTextEdit>
#include <QTextDocument>

namespace TherionStudio
{
BlockEditorDocumentOutlineContext TextEditorTab::blockEditorDocumentOutlineContext() const
{
    BlockEditorDocumentOutlineContext context;
    context.resolveScopeForCommandAtLine = [this](const QString &commandToken, const QStringList &lines, int lineNumber) {
        return resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
    };
    context.isContainerDirectiveInstanceForParsedLine = [this](const QString &directive, const TherionParsedLine &parsedLine) {
        return isContainerDirectiveInstanceForParsedLine(directive, parsedLine);
    };
    context.isCommandDirectiveInScope = [this](const QString &directive, const QString &scopeToken) {
        return isCommandDirectiveInScope(directive, scopeToken);
    };
    return context;
}

BlockEditorDropTargetContext TextEditorTab::blockEditorDropTargetContext() const
{
    BlockEditorDropTargetContext context;
    context.scene = blockCanvasScene_;
    context.blockCanvasItemLineNumber = [this](const QGraphicsItem *item) {
        return blockCanvasItemLineNumber(item);
    };
    context.resolveBlockCanvasItem = [this](QGraphicsItem *item) {
        return resolveBlockCanvasItem(item);
    };
    return context;
}

BlockEditorInsertionPlannerContext TextEditorTab::blockEditorInsertionPlannerContext() const
{
    BlockEditorInsertionPlannerContext context;
    context.blockCanvasItemLineNumber = [this](const QGraphicsItem *item) {
        return blockCanvasItemLineNumber(item);
    };
    context.isCompatibleChildKindForBlocks = [this](const QString &parentKind, const QString &childKind) {
        return isCompatibleChildKindForBlocks(parentKind, childKind);
    };
    return context;
}

BlockEditorMovePlannerContext TextEditorTab::blockEditorMovePlannerContext() const
{
    BlockEditorMovePlannerContext context;
    context.dropTargetContext = blockEditorDropTargetContext();
    context.blockCanvasItemLineNumber = [this](const QGraphicsItem *item) {
        return blockCanvasItemLineNumber(item);
    };
    context.isCompatibleChildKindForBlocks = [this](const QString &parentKind, const QString &childKind) {
        return isCompatibleChildKindForBlocks(parentKind, childKind);
    };
    return context;
}

BlockEditorInsertionContext TextEditorTab::blockEditorInsertionContext()
{
    BlockEditorInsertionContext context;
    context.dialogParent = this;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.documentOutlineContext = blockEditorDocumentOutlineContext();
    context.dropTargetContext = blockEditorDropTargetContext();
    context.insertionPlannerContext = blockEditorInsertionPlannerContext();
    context.isBlocksModeSupportedForCurrentFile = [this]() {
        return isBlocksModeSupportedForCurrentFile();
    };
    context.resolveEndHintContainerStartLineAtScenePos = [this](const QPointF &scenePos) {
        return resolveEndHintContainerStartLineAtScenePos(scenePos);
    };
    context.appendLineNumber = [this]() {
        return editor_ != nullptr ? qMax(1, editor_->document()->blockCount() + 1) : 1;
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

BlockEditorMoveContext TextEditorTab::blockEditorMoveContext()
{
    BlockEditorMoveContext context;
    context.dialogParent = this;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.documentOutlineContext = blockEditorDocumentOutlineContext();
    context.dropTargetContext = blockEditorDropTargetContext();
    context.movePlannerContext = blockEditorMovePlannerContext();
    context.clearBlockMovePreview = [this]() {
        clearBlockMovePreview();
    };
    context.isBlocksModeSupportedForCurrentFile = [this]() {
        return isBlocksModeSupportedForCurrentFile();
    };
    context.resolveEndHintContainerStartLineAtScenePos = [this](const QPointF &scenePos) {
        return resolveEndHintContainerStartLineAtScenePos(scenePos);
    };
    context.isCompatibleChildKindForBlocks = [this](const QString &parentKind, const QString &childKind) {
        return isCompatibleChildKindForBlocks(parentKind, childKind);
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

void TextEditorTab::buildBlockDetailsMovePreviewController()
{
    blockDetailsMovePreviewController_ =
        std::make_unique<BlockEditorMovePreviewController>(blockEditorMovePreviewContext());
}

BlockEditorMovePreviewContext TextEditorTab::blockEditorMovePreviewContext()
{
    BlockEditorMovePreviewContext context;
    context.scene = blockCanvasScene_;
    context.previewLine = &blockMovePreviewLine_;
    context.containerBoundaryEndYByLine = &blockContainerBoundaryEndYByLine_;
    context.resolveBlockCanvasItem = [this](QGraphicsItem *item) {
        return resolveBlockCanvasItem(item);
    };
    context.blockCanvasItemLineNumber = [this](const QGraphicsItem *item) {
        return blockCanvasItemLineNumber(item);
    };
    context.resolveEndHintContainerStartLineAtScenePos = [this](const QPointF &scenePos) {
        return resolveEndHintContainerStartLineAtScenePos(scenePos);
    };
    return context;
}
}
