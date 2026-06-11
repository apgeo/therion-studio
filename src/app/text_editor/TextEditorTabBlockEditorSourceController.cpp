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
    context.sourceContext = [this]() {
        return blockEditorSourceContext();
    };
    context.dataBlockDialogContext = blockEditorDataBlockDialogContext();
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

}
