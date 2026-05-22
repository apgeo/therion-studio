#include "TextEditorTab.h"

#include "TextEditorCursorController.h"

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildCursorController()
{
    TextEditorCursorContext cursorContext;
    cursorContext.editor = editor_;
    cursorContext.searchBar = searchBar_;
    cursorContext.findEdit = findEdit_;
    cursorContext.currentLineNumber = &currentLineNumber_;
    cursorContext.currentColumnNumber = &currentColumnNumber_;
    cursorContext.highlightedLineNumber = &highlightedLineNumber_;
    cursorContext.loading = &loading_;
    cursorContext.blocksModeActive = &blocksModeActive_;
    cursorContext.setBlocksModeActive = [this](bool active) {
        setBlocksModeActive(active);
    };
    cursorContext.currentLineChanged = [this](int lineNumber) {
        emit currentLineChanged(lineNumber);
    };
    cursorContext.cursorPositionChanged = [this](int lineNumber, int columnNumber) {
        emit cursorPositionChanged(lineNumber, columnNumber);
    };
    cursorContext.updateContextHelp = [this]() {
        updateContextHelp();
    };
    cursorContext.updateValidationTooltipForCursor = [this]() {
        updateValidationTooltipForCursor();
    };

    cursorController_ = std::make_unique<TextEditorCursorController>(std::move(cursorContext));
}
}
