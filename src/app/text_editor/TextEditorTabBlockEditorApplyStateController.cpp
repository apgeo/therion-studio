#include "TextEditorTab.h"

#include "block_editor/BlockEditorApplyStateController.h"

#include <memory>

namespace TherionStudio
{
void TextEditorTab::buildBlockDetailsApplyStateController()
{
    blockDetailsApplyStateController_ =
        std::make_unique<BlockEditorApplyStateController>(blockEditorApplyStateContext());
}

BlockEditorApplyStateContext TextEditorTab::blockEditorApplyStateContext()
{
    BlockEditorApplyStateContext context;
    context.tearingDown = &tearingDown_;
    context.detailsPopulating = &blockDetailsPopulating_;
    context.applyButton = &blockDetailsApplyButton_;
    context.statusLabel = &blockDetailsStatusLabel_;
    context.selectedLineNumber = &blockDetailsSelectedLineNumber_;
    context.baseStatusText = &blockDetailsBaseStatusText_;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.buildUpdatedLine = [this](QString *updatedLine, QString *validationError) {
        return buildUpdatedLineFromBlockDetails(updatedLine, validationError);
    };
    return context;
}
}
