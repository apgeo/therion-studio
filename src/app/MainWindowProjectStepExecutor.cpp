#include "MainWindowProjectStepExecutor.h"

namespace TherionStudio
{
void MainWindowProjectStepExecutor::executeOpenProjectSteps(const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> &steps,
                                                            const OpenProjectActions &actions)
{
    for (const MainWindowProjectOrchestrationService::OpenProjectStep step : steps) {
        switch (step) {
        case MainWindowProjectOrchestrationService::OpenProjectStep::ApplyProjectRootToBrowser:
            if (actions.applyProjectRootToBrowser) {
                actions.applyProjectRootToBrowser();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::LoadStructureNameOverrides:
            if (actions.loadStructureNameOverrides) {
                actions.loadStructureNameOverrides();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::SyncOpenDocumentsToProjectRoot:
            if (actions.syncOpenDocumentsToProjectRoot) {
                actions.syncOpenDocumentsToProjectRoot();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::PersistSessionLastProjectPath:
            if (actions.persistSessionLastProjectPath) {
                actions.persistSessionLastProjectPath();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::RebuildStructureSidebar:
            if (actions.rebuildStructureSidebar) {
                actions.rebuildStructureSidebar();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::RefreshTherionConfigDisplay:
            if (actions.refreshTherionConfigDisplay) {
                actions.refreshTherionConfigDisplay();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::UpdateProjectActionState:
            if (actions.updateProjectActionState) {
                actions.updateProjectActionState();
            }
            break;
        case MainWindowProjectOrchestrationService::OpenProjectStep::EnsureWelcomeTab:
            if (actions.ensureWelcomeTab) {
                actions.ensureWelcomeTab();
            }
            break;
        }
    }
}

void MainWindowProjectStepExecutor::executeCloseProjectSteps(const std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> &steps,
                                                             const CloseProjectActions &actions)
{
    for (const MainWindowProjectOrchestrationService::CloseProjectStep step : steps) {
        switch (step) {
        case MainWindowProjectOrchestrationService::CloseProjectStep::ClearDocumentTabs:
            if (actions.clearDocumentTabs) {
                actions.clearDocumentTabs();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::ResetProjectBrowser:
            if (actions.resetProjectBrowser) {
                actions.resetProjectBrowser();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::PersistSessionLastProjectPath:
            if (actions.persistSessionLastProjectPath) {
                actions.persistSessionLastProjectPath();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::PersistOpenDocuments:
            if (actions.persistOpenDocuments) {
                actions.persistOpenDocuments();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::RebuildStructureSidebar:
            if (actions.rebuildStructureSidebar) {
                actions.rebuildStructureSidebar();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::RefreshTherionConfigDisplay:
            if (actions.refreshTherionConfigDisplay) {
                actions.refreshTherionConfigDisplay();
            }
            break;
        case MainWindowProjectOrchestrationService::CloseProjectStep::UpdateProjectActionState:
            if (actions.updateProjectActionState) {
                actions.updateProjectActionState();
            }
            break;
        }
    }
}
}
