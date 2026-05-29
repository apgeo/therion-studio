#include "TextEditorTabInteractionController.h"

#include <QWidget>

namespace TherionStudio
{
bool TextEditorTabInteractionController::handleTextChanged(bool loading,
                                                           const TextChangedActions &actions)
{
    if (loading) {
        return false;
    }

    if (actions.rebuildBlocksCanvasFromText) {
        actions.rebuildBlocksCanvasFromText();
    }
    if (actions.applyDirtyStateFromCurrentState) {
        actions.applyDirtyStateFromCurrentState();
    }
    if (actions.emitDocumentTextChanged) {
        actions.emitDocumentTextChanged();
    }

    return true;
}

void TextEditorTabInteractionController::handleModeRequest(bool blocksModeRequested,
                                                           const ModeRequestActions &actions)
{
    if (actions.setBlocksModeActive) {
        actions.setBlocksModeActive(blocksModeRequested);
    }
}

void TextEditorTabInteractionController::applyModeSelectorVisibility(bool visible,
                                                                     int modeRowSizeHintHeight,
                                                                     const ModeSelectorActions &actions)
{
    if (actions.setModeRowVisible) {
        actions.setModeRowVisible(visible);
    }
    if (actions.setModeRowMaximumHeight) {
        actions.setModeRowMaximumHeight(visible ? QWIDGETSIZE_MAX : 0);
    }
    if (actions.setModeRowMinimumHeight) {
        actions.setModeRowMinimumHeight(visible ? modeRowSizeHintHeight : 0);
    }
    if (actions.invalidateRootLayout) {
        actions.invalidateRootLayout();
    }
    if (actions.activateRootLayout) {
        actions.activateRootLayout();
    }
}
}
