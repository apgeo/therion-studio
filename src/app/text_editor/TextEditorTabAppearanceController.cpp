#include "TextEditorTab.h"

#include "TextEditorAppearanceController.h"

#include <QFrame>

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildAppearanceController()
{
    TextEditorAppearanceContext appearanceContext;
    appearanceContext.rootWidget = this;
    appearanceContext.editor = editor_;
    appearanceContext.blockCanvasView = blockCanvasView_;
    appearanceContext.blockCanvasScene = blockCanvasScene_;
    appearanceContext.helpPanel = helpPanel_;
    appearanceContext.blockDetailsPanel = blockDetailsPanel_;
    appearanceContext.blockDetailsEditPanel = blockDetailsEditPanel_;
    appearanceContext.blockDetailsHelpPanel = blockDetailsHelpPanel_;
    appearanceContext.helpBrowser = helpBrowser_;
    appearanceContext.blockDetailsHelpBrowser = blockDetailsHelpBrowser_;
    appearanceContext.blocksModeActive = &blocksModeActive_;
    appearanceContext.updateContextHelp = [this]() {
        updateContextHelp();
    };
    appearanceContext.updateBlockDetailsHelpForCurrentFocus = [this]() {
        updateBlockDetailsHelpForCurrentFocus();
    };

    appearanceController_ = std::make_unique<TextEditorAppearanceController>(std::move(appearanceContext));
}
}
