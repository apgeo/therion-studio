#include "TextEditorTab.h"

#include "block_editor/BlockEditorApplyExecutor.h"

#include <memory>

namespace TherionStudio
{
void TextEditorTab::buildBlockDetailsApplyExecutor()
{
    blockDetailsApplyExecutor_ =
        std::make_unique<BlockEditorApplyExecutor>(blockEditorApplyExecutorContext());
}

BlockEditorApplyExecutorContext TextEditorTab::blockEditorApplyExecutorContext()
{
    BlockEditorApplyExecutorContext context;
    context.dialogParent = this;
    context.tearingDown = &tearingDown_;
    context.selectedLineNumber = &blockDetailsSelectedLineNumber_;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.buildUpdatedLine = [this](QString *updatedLine, QString *validationError) {
        return buildUpdatedLineFromBlockDetails(updatedLine, validationError);
    };
    context.selectBlockInCanvasAndDetails = [this](int lineNumber) {
        selectBlockInCanvasAndDetails(lineNumber);
    };
    context.refreshApplyState = [this]() {
        refreshBlockDetailsApplyState();
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}
}
