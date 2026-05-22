#include "TextEditorTab.h"

#include "TextEditorDocumentController.h"

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildDocumentController()
{
    TextEditorDocumentContext documentContext;
    documentContext.editor = editor_;
    documentContext.filePath = &filePath_;
    documentContext.projectRootPath = &projectRootPath_;
    documentContext.fileEncodingName = &fileEncodingName_;
    documentContext.fileEncodingLabel = &fileEncodingLabel_;
    documentContext.encodingStatusNote = &encodingStatusNote_;
    documentContext.cleanTextSnapshot = &cleanTextSnapshot_;
    documentContext.cleanEncodingNameSnapshot = &cleanEncodingNameSnapshot_;
    documentContext.blockDetailsSelectedKind = &blockDetailsSelectedKind_;
    documentContext.loading = &loading_;
    documentContext.dirty = &dirty_;
    documentContext.blocksModeActive = &blocksModeActive_;
    documentContext.currentLineNumber = &currentLineNumber_;
    documentContext.currentColumnNumber = &currentColumnNumber_;
    documentContext.highlightedLineNumber = &highlightedLineNumber_;
    documentContext.blockDetailsSelectedLineNumber = &blockDetailsSelectedLineNumber_;
    documentContext.translate = [this](const char *text) {
        return tr(text);
    };
    documentContext.refreshBlocksModeAvailability = [this]() {
        refreshBlocksModeAvailability();
    };
    documentContext.isBlocksModeSupportedForCurrentFile = [this]() {
        return isBlocksModeSupportedForCurrentFile();
    };
    documentContext.setBlocksModeActive = [this](bool active) {
        setBlocksModeActive(active);
    };
    documentContext.rebuildBlocksCanvasFromText = [this]() {
        rebuildBlocksCanvasFromText();
    };
    documentContext.clearBlockDetailsPane = [this]() {
        clearBlockDetailsPane();
    };
    documentContext.populateBlockToolbox = [this]() {
        populateBlockToolbox();
    };
    documentContext.refreshEditorModeUi = [this]() {
        refreshEditorModeUi();
    };
    documentContext.refreshTitle = [this]() {
        refreshTitle();
    };
    documentContext.refreshCurrentLineHighlight = [this]() {
        refreshCurrentLineHighlight();
    };
    documentContext.dirtyStateChanged = [this](bool dirty) {
        emit dirtyStateChanged(dirty);
    };
    documentContext.updateContextHelp = [this]() {
        updateContextHelp();
    };
    documentContext.refreshStatus = [this]() {
        refreshStatus();
    };

    documentController_ = std::make_unique<TextEditorDocumentController>(std::move(documentContext));
}
}
