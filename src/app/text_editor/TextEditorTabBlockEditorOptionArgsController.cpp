#include "TextEditorTab.h"

#include "block_editor/BlockEditorOptionArgsController.h"

#include <memory>

namespace TherionStudio
{
void TextEditorTab::buildBlockDetailsOptionArgsController()
{
    blockDetailsOptionArgsController_ =
        std::make_unique<BlockEditorOptionArgsController>(blockEditorOptionArgsContext());
}

BlockEditorOptionArgsContext TextEditorTab::blockEditorOptionArgsContext()
{
    BlockEditorOptionArgsContext context;
    context.signalContext = this;
    context.optionArgsLabel = blockDetailsOptionArgsLabel_;
    context.optionArgsPanel = blockDetailsOptionArgsPanel_;
    context.optionArgsFormLayout = blockDetailsOptionArgsFormLayout_;
    context.optionsTable = blockDetailsOptionsTable_;
    context.optionArgEditors = &blockDetailsOptionArgEditors_;
    context.optionArgsSyncing = &blockDetailsOptionArgsSyncing_;
    context.commandMetadata = &commandMetadata_;
    context.isStructuredOptionsMode = [this]() {
        return blockDetailsMode_ == BlockDetailsMode::StructuredOptions;
    };
    context.selectedKind = [this]() {
        return blockDetailsSelectedKind_;
    };
    context.normalizeDirectiveToken = [this](const QString &directive) {
        return normalizedDirectiveToken(directive);
    };
    context.refreshApplyState = [this]() {
        refreshBlockDetailsApplyState();
    };
    context.applyChanges = [this]() {
        applyBlockDetailsChanges();
    };
    context.updateHelpForCurrentFocus = [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}
}
