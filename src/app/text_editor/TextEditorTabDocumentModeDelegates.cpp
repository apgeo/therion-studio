#include "TextEditorTab.h"

#include "TextEditorDocumentController.h"
#include "TextEditorModeController.h"
#include "TextEditorTabInteractionController.h"

#include <QLayout>

namespace TherionStudio
{
bool TextEditorTab::loadFile(const QString &filePath, QString *errorMessage)
{
    return documentController_ != nullptr
        && documentController_->loadFile(filePath, errorMessage);
}

void TextEditorTab::setProjectRootPath(const QString &projectRootPath)
{
    if (documentController_ != nullptr) {
        documentController_->setProjectRootPath(projectRootPath);
    }
}

bool TextEditorTab::save(QString *errorMessage)
{
    return documentController_ != nullptr
        && documentController_->save(errorMessage);
}

void TextEditorTab::setModeSelectorVisible(bool visible)
{
    modeSelectorRequestedVisible_ = visible;
    TextEditorTabInteractionController::ModeSelectorActions actions;
    if (modeRow_ != nullptr) {
        actions.setModeRowVisible = [this](bool rowVisible) {
            modeRow_->setVisible(rowVisible);
        };
        actions.setModeRowMaximumHeight = [this](int maximumHeight) {
            modeRow_->setMaximumHeight(maximumHeight);
        };
        actions.setModeRowMinimumHeight = [this](int minimumHeight) {
            modeRow_->setMinimumHeight(minimumHeight);
        };
    }

    if (QLayout *rootLayout = layout(); rootLayout != nullptr) {
        actions.invalidateRootLayout = [rootLayout]() {
            rootLayout->invalidate();
        };
        actions.activateRootLayout = [rootLayout]() {
            rootLayout->activate();
        };
    }

    const int modeRowSizeHintHeight = modeRow_ == nullptr ? 0 : modeRow_->sizeHint().height();
    TextEditorTabInteractionController::applyModeSelectorVisibility(modeSelectorRequestedVisible_,
                                                                    modeRowSizeHintHeight,
                                                                    actions);
}

TextEditorTab::EditorMode TextEditorTab::editorMode() const
{
    return blocksModeActive_ ? EditorMode::Blocks : EditorMode::Raw;
}

bool TextEditorTab::isBlocksModeAvailable() const
{
    return isBlocksModeSupportedForCurrentFile();
}

void TextEditorTab::setEditorMode(EditorMode mode)
{
    setBlocksModeActive(mode == EditorMode::Blocks);
}

bool TextEditorTab::isBlocksModeSupportedForCurrentFile() const
{
    return editorModeController_ != nullptr
        && editorModeController_->isBlocksModeSupportedForCurrentFile();
}

void TextEditorTab::refreshBlocksModeAvailability()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshBlocksModeAvailability();
    }
}

void TextEditorTab::setBlocksModeActive(bool active)
{
    if (editorModeController_ != nullptr) {
        editorModeController_->setBlocksModeActive(active);
    }
}

bool TextEditorTab::ensureEncodingRootDirectiveForBlocks()
{
    return editorModeController_ != nullptr
        && editorModeController_->ensureEncodingRootDirectiveForBlocks();
}

void TextEditorTab::refreshEditorModeUi()
{
    if (editorModeController_ != nullptr) {
        editorModeController_->refreshEditorModeUi();
    }
}

void TextEditorTab::handleRawModeRequested()
{
    TextEditorTabInteractionController::ModeRequestActions actions;
    actions.setBlocksModeActive = [this](bool active) {
        setBlocksModeActive(active);
    };
    TextEditorTabInteractionController::handleModeRequest(false, actions);
}

void TextEditorTab::handleBlocksModeRequested()
{
    TextEditorTabInteractionController::ModeRequestActions actions;
    actions.setBlocksModeActive = [this](bool active) {
        setBlocksModeActive(active);
    };
    TextEditorTabInteractionController::handleModeRequest(true, actions);
}
}
