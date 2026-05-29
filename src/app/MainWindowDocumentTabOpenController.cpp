#include "MainWindowDocumentTabOpenController.h"

namespace TherionStudio
{
bool MainWindowDocumentTabOpenController::activateExistingTab(const ActivateExistingTabRequest &request,
                                                              const Actions &actions)
{
    if (request.tabIndex < 0) {
        return false;
    }

    if (actions.setCurrentTabIndex) {
        actions.setCurrentTabIndex(request.tabIndex);
    }
    if (actions.refreshDocumentStatusWidgets) {
        actions.refreshDocumentStatusWidgets();
    }
    if (actions.refreshWorkspaceModeSwitcher) {
        actions.refreshWorkspaceModeSwitcher();
    }

    return true;
}

bool MainWindowDocumentTabOpenController::attachNewTab(const AttachNewTabRequest &request,
                                                       const Actions &actions)
{
    if (actions.removeWelcomeTabIfPresent) {
        actions.removeWelcomeTabIfPresent();
    }

    if (!actions.addTab) {
        return false;
    }

    const int tabIndex = actions.addTab(request.tabTitle);
    if (tabIndex < 0) {
        return false;
    }

    if (actions.setCurrentTabIndex) {
        actions.setCurrentTabIndex(tabIndex);
    }
    if (actions.handleCurrentLineChanged) {
        actions.handleCurrentLineChanged(request.openedDocumentPath, request.currentLineNumber);
    }
    if (actions.updateCurrentTabTitle) {
        actions.updateCurrentTabTitle();
    }
    if (actions.refreshDocumentStatusWidgets) {
        actions.refreshDocumentStatusWidgets();
    }
    if (actions.refreshWorkspaceModeSwitcher) {
        actions.refreshWorkspaceModeSwitcher();
    }
    if (actions.persistOpenDocuments) {
        actions.persistOpenDocuments();
    }
    if (!request.consoleOpenedLine.isEmpty() && actions.appendConsoleLine) {
        actions.appendConsoleLine(request.consoleOpenedLine);
    }

    return true;
}
}
