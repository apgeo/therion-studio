#include "TextEditorTab.h"

#include "block_editor/BlockEditorConfigureController.h"
#include "block_editor/BlockEditorDeleteExecutor.h"
#include "block_editor/BlockEditorSourceController.h"

namespace TherionStudio
{
BlockEditorSourceContext TextEditorTab::blockEditorSourceContext()
{
    BlockEditorSourceContext context;
    context.editor = editor_;
    context.editable = true;
    context.replaceText = [this](const QString &contents) {
        replaceTextForCommand(contents);
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

BlockEditorSourceContext TextEditorTab::blockEditorSourceContext() const
{
    BlockEditorSourceContext context;
    context.editor = editor_;
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

BlockEditorDeleteContext TextEditorTab::blockEditorDeleteContext()
{
    BlockEditorDeleteContext context;
    context.dialogParent = this;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.resolveScopeForCommandAtLine = [this](const QString &commandToken, const QStringList &lines, int lineNumber) {
        return resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
    };
    context.isCommandDirectiveInScope = [this](const QString &directive, const QString &scopeToken) {
        return isCommandDirectiveInScope(directive, scopeToken);
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

BlockEditorConfigureContext TextEditorTab::blockEditorConfigureContext()
{
    BlockEditorConfigureContext context;
    context.dialogParent = this;
    context.commandMetadata = &commandMetadata_;
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.commandSupportsInlineIdField = [this](const QString &commandToken) {
        return commandSupportsInlineIdField(commandToken);
    };
    context.commandHasRequiredIdArgument = [this](const QString &commandToken) {
        return commandHasRequiredIdArgument(commandToken);
    };
    context.commandOptionsDialogContext = blockEditorCommandOptionsDialogContext();
    context.dataBlockDialogContext = blockEditorDataBlockDialogContext();
    context.singleValueCommandDialogContext = blockEditorSingleValueCommandDialogContext();
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

BlockEditorCommandOptionsDialogContext TextEditorTab::blockEditorCommandOptionsDialogContext()
{
    BlockEditorCommandOptionsDialogContext context;
    context.dialogParent = this;
    context.commandMetadata = &commandMetadata_;
    context.isRequiredArgumentSignatureForBlocks = [this](const QString &signature) {
        return isRequiredArgumentSignatureForBlocks(signature);
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

BlockEditorDataBlockDialogContext TextEditorTab::blockEditorDataBlockDialogContext()
{
    BlockEditorDataBlockDialogContext context;
    context.dialogParent = this;
    context.commandMetadata = &commandMetadata_;
    context.resolveScopeForCommandAtLine = [this](const QString &commandToken, const QStringList &lines, int lineNumber) {
        return resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
    };
    context.isCommandDirectiveInScope = [this](const QString &directive, const QString &scopeToken) {
        return isCommandDirectiveInScope(directive, scopeToken);
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}

BlockEditorSingleValueCommandDialogContext TextEditorTab::blockEditorSingleValueCommandDialogContext()
{
    BlockEditorSingleValueCommandDialogContext context;
    context.dialogParent = this;
    context.commandMetadata = &commandMetadata_;
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}
}
