#include "MainWindowProjectController.h"

#include "MainWindowProjectLifecycleService.h"
#include "MainWindowProjectOrchestrationService.h"
#include "MainWindowProjectStepExecutor.h"
#include "MainWindowProjectUiFlowService.h"
#include "MainWindowProjectWorkspaceService.h"

namespace TherionStudio
{
void MainWindowProjectController::openProject(const QString &selectedProjectPath,
                                              bool hasNoTabs,
                                              bool hasWelcomeTab,
                                              ISessionStore &sessionStore,
                                              const Actions &actions)
{
    const MainWindowProjectLifecycleService::OpenProjectDecision decision =
        MainWindowProjectLifecycleService::decideOpenProject(selectedProjectPath);
    const MainWindowProjectUiFlowService::DecisionPresentation decisionPresentation =
        MainWindowProjectUiFlowService::presentOpenProjectDecision(decision);
    if (!decisionPresentation.shouldContinueWorkflow) {
        if (decisionPresentation.showWarningDialog && actions.showWarningDialog) {
            actions.showWarningDialog(decisionPresentation.warningDialogTitle,
                                      decisionPresentation.warningDialogMessage);
        }
        if (decisionPresentation.showStatusBarMessage && actions.showStatusBarMessage) {
            actions.showStatusBarMessage(decisionPresentation.statusBarMessage,
                                         decisionPresentation.statusBarTimeoutMs);
        }
        return;
    }

    const MainWindowProjectWorkspaceService::OpenProjectWorkspaceState workspaceState =
        MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
            decision.projectPath, hasNoTabs, hasWelcomeTab);
    if (actions.setProjectRootPath) {
        actions.setProjectRootPath(workspaceState.projectRootPath);
    }

    MainWindowProjectStepExecutor::OpenProjectActions openActions;
    openActions.applyProjectRootToBrowser = [&actions, &workspaceState]() {
        if (actions.applyProjectRootToBrowser) {
            actions.applyProjectRootToBrowser(workspaceState.projectRootPath);
        }
    };
    openActions.loadStructureNameOverrides = actions.loadStructureNameOverrides;
    openActions.syncOpenDocumentsToProjectRoot = actions.syncOpenDocumentsToProjectRoot;
    openActions.persistSessionLastProjectPath = [&sessionStore, &workspaceState]() {
        sessionStore.setLastProjectPath(workspaceState.sessionLastProjectPath);
    };
    openActions.rebuildStructureSidebar = actions.rebuildStructureSidebar;
    openActions.refreshTherionConfigDisplay = actions.refreshTherionConfigDisplay;
    openActions.updateProjectActionState = actions.updateProjectActionState;
    openActions.ensureWelcomeTab = actions.ensureWelcomeTab;
    MainWindowProjectStepExecutor::executeOpenProjectSteps(
        MainWindowProjectOrchestrationService::buildOpenProjectSteps(workspaceState.shouldEnsureWelcomeTab),
        openActions);

    const MainWindowProjectUiFlowService::SuccessPresentation successPresentation =
        MainWindowProjectUiFlowService::presentOpenProjectSuccess(workspaceState.projectRootPath);
    if (actions.showStatusBarMessage) {
        actions.showStatusBarMessage(successPresentation.statusBarMessage,
                                     successPresentation.statusBarTimeoutMs);
    }
    if (actions.appendConsoleLine) {
        actions.appendConsoleLine(successPresentation.consoleMessage);
    }
}

void MainWindowProjectController::closeProject(const QString &currentProjectPath,
                                               const std::function<bool()> &confirmCloseDirtyDocuments,
                                               ISessionStore &sessionStore,
                                               const Actions &actions)
{
    const MainWindowProjectLifecycleService::CloseProjectDecision decision =
        MainWindowProjectLifecycleService::decideCloseProject(currentProjectPath,
                                                              confirmCloseDirtyDocuments);
    const MainWindowProjectUiFlowService::DecisionPresentation decisionPresentation =
        MainWindowProjectUiFlowService::presentCloseProjectDecision(decision);
    if (!decisionPresentation.shouldContinueWorkflow) {
        if (decisionPresentation.showStatusBarMessage && actions.showStatusBarMessage) {
            actions.showStatusBarMessage(decisionPresentation.statusBarMessage,
                                         decisionPresentation.statusBarTimeoutMs);
        }
        return;
    }

    const MainWindowProjectWorkspaceService::CloseProjectWorkspaceState workspaceState =
        MainWindowProjectWorkspaceService::buildCloseProjectWorkspaceState(
            decision.closedProjectPath);
    if (actions.setProjectRootPath) {
        actions.setProjectRootPath(workspaceState.projectRootPath);
    }

    MainWindowProjectStepExecutor::CloseProjectActions closeActions;
    closeActions.clearDocumentTabs = actions.clearDocumentTabs;
    closeActions.resetProjectBrowser = actions.resetProjectBrowser;
    closeActions.persistSessionLastProjectPath = [&sessionStore, &workspaceState]() {
        sessionStore.setLastProjectPath(workspaceState.sessionLastProjectPath);
    };
    closeActions.persistOpenDocuments = actions.persistOpenDocuments;
    closeActions.rebuildStructureSidebar = actions.rebuildStructureSidebar;
    closeActions.refreshTherionConfigDisplay = actions.refreshTherionConfigDisplay;
    closeActions.updateProjectActionState = actions.updateProjectActionState;
    MainWindowProjectStepExecutor::executeCloseProjectSteps(
        MainWindowProjectOrchestrationService::buildCloseProjectSteps(),
        closeActions);

    const MainWindowProjectUiFlowService::SuccessPresentation successPresentation =
        MainWindowProjectUiFlowService::presentCloseProjectSuccess(workspaceState.closedProjectPath);
    if (actions.showStatusBarMessage) {
        actions.showStatusBarMessage(successPresentation.statusBarMessage,
                                     successPresentation.statusBarTimeoutMs);
    }
    if (actions.appendConsoleLine) {
        actions.appendConsoleLine(successPresentation.consoleMessage);
    }
}
}
