#include "TextEditorTab.h"

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
}
