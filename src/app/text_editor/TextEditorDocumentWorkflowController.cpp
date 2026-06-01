#include "TextEditorDocumentWorkflowController.h"

namespace TherionStudio
{
void TextEditorDocumentWorkflowController::runPostLoadWorkflow(bool disableBlocksMode,
                                                               const LoadActions &actions)
{
    if (actions.refreshBlocksModeAvailability) {
        actions.refreshBlocksModeAvailability();
    }
    if (disableBlocksMode && actions.setBlocksModeActive) {
        actions.setBlocksModeActive(false);
    }
    if (actions.rebuildBlocksCanvasFromText) {
        actions.rebuildBlocksCanvasFromText();
    }
    if (actions.clearBlockDetailsPane) {
        actions.clearBlockDetailsPane();
    }
    if (actions.populateBlockToolbox) {
        actions.populateBlockToolbox();
    }
    if (actions.refreshEditorModeUi) {
        actions.refreshEditorModeUi();
    }
    if (actions.refreshTitle) {
        actions.refreshTitle();
    }
    if (actions.refreshStatus) {
        actions.refreshStatus();
    }
    if (actions.refreshCurrentLineHighlight) {
        actions.refreshCurrentLineHighlight();
    }
    if (actions.dirtyStateChanged) {
        actions.dirtyStateChanged(false);
    }
    if (actions.updateContextHelp) {
        actions.updateContextHelp();
    }
}

void TextEditorDocumentWorkflowController::runPostSaveWorkflow(const SaveActions &actions)
{
    if (actions.refreshTitle) {
        actions.refreshTitle();
    }
    if (actions.refreshStatus) {
        actions.refreshStatus();
    }
    if (actions.dirtyStateChanged) {
        actions.dirtyStateChanged(false);
    }
}

void TextEditorDocumentWorkflowController::runPostProjectRootSetWorkflow(const ProjectRootActions &actions)
{
    if (actions.refreshStatus) {
        actions.refreshStatus();
    }
}
}
