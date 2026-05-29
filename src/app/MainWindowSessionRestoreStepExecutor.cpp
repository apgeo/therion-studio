#include "MainWindowSessionRestoreStepExecutor.h"

namespace TherionStudio
{
void MainWindowSessionRestoreStepExecutor::execute(const MainWindowSessionRestoreOrchestrationService::Plan &plan,
                                                   const Actions &actions)
{
    const QString &projectPath = plan.restoreProjectDecision.projectPath;

    for (const MainWindowSessionRestoreOrchestrationService::Step step : plan.steps) {
        switch (step) {
        case MainWindowSessionRestoreOrchestrationService::Step::ApplyProjectRootToBrowser:
            if (actions.applyProjectRootToBrowser) {
                actions.applyProjectRootToBrowser(projectPath);
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::AppendRestoredProjectRootConsole:
            if (actions.appendRestoredProjectRootConsole) {
                actions.appendRestoredProjectRootConsole(projectPath);
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::LoadStructureNameOverrides:
            if (actions.loadStructureNameOverrides) {
                actions.loadStructureNameOverrides();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::SyncOpenDocumentsToProjectRoot:
            if (actions.syncOpenDocumentsToProjectRoot) {
                actions.syncOpenDocumentsToProjectRoot();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::RebuildStructureSidebar:
            if (actions.rebuildStructureSidebar) {
                actions.rebuildStructureSidebar();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::AppendSkippedProtectedProjectConsole:
            if (actions.appendSkippedProtectedProjectConsole) {
                actions.appendSkippedProtectedProjectConsole(projectPath);
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay:
            if (actions.refreshTherionConfigDisplay) {
                actions.refreshTherionConfigDisplay();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState:
            if (actions.updateProjectActionState) {
                actions.updateProjectActionState();
            }
            break;
        case MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments:
            if (actions.restoreOpenDocuments) {
                actions.restoreOpenDocuments();
            }
            break;
        }
    }
}
}
