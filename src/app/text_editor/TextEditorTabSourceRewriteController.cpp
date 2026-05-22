#include "TextEditorTab.h"

#include "TextEditorSourceRewriteController.h"

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildSourceRewriteController()
{
    TextEditorSourceRewriteContext sourceRewriteContext;
    sourceRewriteContext.editor = editor_;
    sourceRewriteContext.loading = &loading_;
    sourceRewriteContext.currentLineNumber = &currentLineNumber_;
    sourceRewriteContext.applyDirtyStateFromCurrentState = [this]() {
        applyDirtyStateFromCurrentState();
    };
    sourceRewriteContext.rebuildBlocksCanvasFromText = [this]() {
        rebuildBlocksCanvasFromText();
    };
    sourceRewriteContext.documentTextChanged = [this]() {
        emit documentTextChanged();
    };
    sourceRewriteContext.updateContextHelp = [this]() {
        updateContextHelp();
    };

    sourceRewriteController_ =
        std::make_unique<TextEditorSourceRewriteController>(std::move(sourceRewriteContext));
}
}
