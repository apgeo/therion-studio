#include "TextEditorTab.h"

#include "block_editor/BlockEditorToolboxController.h"

#include <memory>

namespace TherionStudio
{
void TextEditorTab::buildBlockToolboxController()
{
    blockToolboxController_ = std::make_unique<BlockEditorToolboxController>(blockEditorToolboxContext());
}

BlockEditorToolboxContext TextEditorTab::blockEditorToolboxContext()
{
    BlockEditorToolboxContext context;
    context.tearingDown = &tearingDown_;
    context.filterEdit = &blockToolboxFilterEdit_;
    context.toolboxList = &blockToolboxList_;
    context.scopeCombo = &blockToolboxScopeCombo_;
    context.canvasScene = &blockCanvasScene_;
    context.editor = &editor_;
    context.commandMetadata = &commandMetadata_;
    context.normalizeCompletionContext = [this](const QString &contextToken) {
        return normalizeCompletionContext(contextToken);
    };
    context.isCompatibleChildKindForBlocks = [this](const QString &parentKind, const QString &childKind) {
        return isCompatibleChildKindForBlocks(parentKind, childKind);
    };
    context.translate = [this](const char *text) {
        return tr(text);
    };
    return context;
}
}
