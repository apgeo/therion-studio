#include "MainWindowSessionController.h"

#include "MainWindowSessionProjectService.h"
#include "MainWindowSessionRestoreOrchestrationService.h"
#include "MainWindowSessionRestoreStepExecutor.h"
#include "MainWindowSessionRestoreUiFlowService.h"
#include "MainWindowSessionWindowRestoreService.h"

namespace TherionStudio
{
void MainWindowSessionController::restoreSession(const ISessionStore &sessionStore,
                                                 const RestoreSessionActions &actions)
{
    const QByteArray geometry = sessionStore.mainWindowGeometry();
    const QByteArray state = sessionStore.mainWindowState();
    const MainWindowSessionWindowRestoreService::Plan windowRestorePlan =
        MainWindowSessionWindowRestoreService::buildPlan(geometry, state);

    bool geometryRestoreSucceeded = false;
    if (windowRestorePlan.shouldAttemptRestoreGeometry && actions.restoreGeometry) {
        geometryRestoreSucceeded = actions.restoreGeometry(geometry);
    }
    if (MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(
            windowRestorePlan, geometryRestoreSucceeded)
        && actions.resizeToDefault) {
        actions.resizeToDefault();
    }
    if (windowRestorePlan.shouldAttemptRestoreState && actions.restoreState) {
        actions.restoreState(state);
    }
    if (windowRestorePlan.shouldEnsureUsableWindowSize && actions.ensureUsableWindowSize) {
        actions.ensureUsableWindowSize();
    }

    const MainWindowSessionProjectService::ProjectRestoreDecision restoreProjectDecision =
        MainWindowSessionProjectService::decideProjectRestore(sessionStore.lastProjectPath());
    const MainWindowSessionRestoreOrchestrationService::Plan restorePlan =
        MainWindowSessionRestoreOrchestrationService::buildPlan(restoreProjectDecision);

    MainWindowSessionRestoreStepExecutor::Actions restoreActions;
    restoreActions.applyProjectRootToBrowser = actions.applyProjectRootToBrowser;
    restoreActions.appendRestoredProjectRootConsole = [&actions](const QString &projectPath) {
        if (actions.appendConsoleLine) {
            actions.appendConsoleLine(MainWindowSessionRestoreUiFlowService::restoredProjectRootConsoleLine(projectPath));
        }
    };
    restoreActions.loadStructureNameOverrides = actions.loadStructureNameOverrides;
    restoreActions.syncOpenDocumentsToProjectRoot = actions.syncOpenDocumentsToProjectRoot;
    restoreActions.rebuildStructureSidebar = actions.rebuildStructureSidebar;
    restoreActions.appendSkippedProtectedProjectConsole = [&actions](const QString &projectPath) {
        if (actions.appendConsoleLine) {
            actions.appendConsoleLine(MainWindowSessionRestoreUiFlowService::skippedProtectedProjectConsoleLine(projectPath));
        }
    };
    restoreActions.refreshTherionConfigDisplay = actions.refreshTherionConfigDisplay;
    restoreActions.updateProjectActionState = actions.updateProjectActionState;
    restoreActions.restoreOpenDocuments = actions.restoreOpenDocuments;
    MainWindowSessionRestoreStepExecutor::execute(restorePlan, restoreActions);
}
}
