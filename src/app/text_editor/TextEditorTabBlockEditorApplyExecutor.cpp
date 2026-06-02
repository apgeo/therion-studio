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
    context.tearingDown = &tearingDown_;
    context.detailsPopulating = &blockDetailsPopulating_;
    context.selectedLineNumber = &blockDetailsSelectedLineNumber_;
    context.baseStatusText = &blockDetailsBaseStatusText_;
    context.statusLabel = blockDetailsStatusLabel_;
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
    return context;
}
}
