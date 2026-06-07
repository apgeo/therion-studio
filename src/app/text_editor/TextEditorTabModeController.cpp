#include "TextEditorTab.h"

#include "TextEditorModeController.h"

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildModeController()
{
    TextEditorModeContext modeContext;
    modeContext.filePath = &filePath_;
    modeContext.untitledDisplayName = &untitledDisplayName_;
    modeContext.fileEncodingName = &fileEncodingName_;
    modeContext.blocksModeActive = &blocksModeActive_;
    modeContext.enforcingEncodingRootDirective = &enforcingEncodingRootDirective_;
    modeContext.editor = editor_;
    modeContext.rawModeButton = rawModeButton_;
    modeContext.blocksModeButton = blocksModeButton_;
    modeContext.editorModeStack = editorModeStack_;
    modeContext.rawEditorPanel = rawEditorPanel_;
    modeContext.blocksPanel = blocksPanel_;
    modeContext.hideFindBar = [this]() {
        hideFindBar();
    };
    modeContext.replaceTextForSystemNormalization = [this](const QString &contents) {
        replaceTextForSystemNormalization(contents);
    };
    modeContext.rebuildBlocksCanvasFromText = [this]() {
        rebuildBlocksCanvasFromText();
    };
    modeContext.populateBlockToolbox = [this]() {
        populateBlockToolbox();
    };
    modeContext.editorModeChanged = [this]() {
        emit editorModeChanged(editorMode());
    };

    editorModeController_ = std::make_unique<TextEditorModeController>(std::move(modeContext));
}
}
