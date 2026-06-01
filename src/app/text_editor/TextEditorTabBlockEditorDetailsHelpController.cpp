#include "TextEditorTab.h"

#include "block_editor/BlockEditorDetailsHelpController.h"

#include <memory>

namespace TherionStudio
{
void TextEditorTab::buildBlockDetailsHelpController()
{
    blockDetailsHelpController_ =
        std::make_unique<BlockEditorDetailsHelpController>(blockEditorDetailsHelpContext());
}

BlockEditorDetailsHelpContext TextEditorTab::blockEditorDetailsHelpContext()
{
    BlockEditorDetailsHelpContext context;
    context.tearingDown = &tearingDown_;
    context.helpBrowser = blockDetailsHelpBrowser_;
    context.helpInspector = blockDetailsHelpInspector_;
    context.idEdit = blockDetailsIdEdit_;
    context.primaryFieldLabel = blockDetailsPrimaryFieldLabel_;
    context.additionalPositionalEdit = blockDetailsAdditionalPositionalEdit_;
    context.commentEdit = blockDetailsCommentEdit_;
    context.commentMarker = &blockDetailsCommentMarker_;
    context.commandMetadata = &commandMetadata_;
    context.selectedKind = [this]() {
        return blockDetailsSelectedKind_;
    };
    context.normalizeDirectiveToken = [this](const QString &directive) {
        return normalizedDirectiveToken(directive);
    };
    context.isRequiredArgumentSignature = [this](const QString &signature) {
        return isRequiredArgumentSignatureForBlocks(signature);
    };
    context.readingsTagEditorHasEditorFocus = [this]() {
        return blockDetailsReadingsTagEditorHasEditorFocus();
    };
    context.isStructuredOptionsMode = [this]() {
        return blockDetailsMode_ == BlockDetailsMode::StructuredOptions;
    };
    context.isSimpleValueMode = [this]() {
        return blockDetailsMode_ == BlockDetailsMode::SimpleValue;
    };
    context.isDataHeaderMode = [this]() {
        return blockDetailsMode_ == BlockDetailsMode::DataHeader;
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}
}
