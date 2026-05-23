#include "TextEditorTab.h"

#include "block_editor/BlockEditorLineBuildService.h"

#include <memory>

namespace TherionStudio
{
void TextEditorTab::buildBlockDetailsLineBuildService()
{
    blockDetailsLineBuildService_ =
        std::make_unique<BlockEditorLineBuildService>(blockEditorLineBuildContext());
}

BlockEditorLineBuildContext TextEditorTab::blockEditorLineBuildContext()
{
    BlockEditorLineBuildContext context;
    context.selectedLineNumber = &blockDetailsSelectedLineNumber_;
    context.selectedKind = &blockDetailsSelectedKind_;
    context.idEdit = blockDetailsIdEdit_;
    context.additionalPositionalEdit = blockDetailsAdditionalPositionalEdit_;
    context.commentEdit = blockDetailsCommentEdit_;
    context.optionsTable = blockDetailsOptionsTable_;
    context.commentMarker = &blockDetailsCommentMarker_;
    context.commandMetadata = &commandMetadata_;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.readingsOrderTextForBuild = [this]() {
        return blockDetailsReadingsOrderTextForBuild();
    };
    context.normalizeDirectiveToken = [this](const QString &directive) {
        return normalizedDirectiveToken(directive);
    };
    context.commandHasRequiredIdArgument = [this](const QString &commandToken) {
        return commandHasRequiredIdArgument(commandToken);
    };
    context.isSimpleValueMode = [this]() {
        return blockDetailsMode_ == BlockDetailsMode::SimpleValue;
    };
    context.isDataHeaderMode = [this]() {
        return blockDetailsMode_ == BlockDetailsMode::DataHeader;
    };
    context.isStructuredOptionsMode = [this]() {
        return blockDetailsMode_ == BlockDetailsMode::StructuredOptions;
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}
}
