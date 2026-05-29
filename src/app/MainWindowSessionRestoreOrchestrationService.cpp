#include "MainWindowSessionRestoreOrchestrationService.h"

namespace TherionStudio
{
MainWindowSessionRestoreOrchestrationService::Plan MainWindowSessionRestoreOrchestrationService::buildPlan(
    const MainWindowSessionProjectService::ProjectRestoreDecision &restoreProjectDecision)
{
    Plan plan;
    plan.restoreProjectDecision = restoreProjectDecision;

    if (restoreProjectDecision.status == MainWindowSessionProjectService::ProjectRestoreStatus::Restored) {
        plan.steps = {
            Step::ApplyProjectRootToBrowser,
            Step::AppendRestoredProjectRootConsole,
            Step::LoadStructureNameOverrides,
            Step::SyncOpenDocumentsToProjectRoot,
            Step::RebuildStructureSidebar,
            Step::RefreshTherionConfigDisplay,
            Step::UpdateProjectActionState,
            Step::RestoreOpenDocuments};
        return plan;
    }

    if (restoreProjectDecision.status == MainWindowSessionProjectService::ProjectRestoreStatus::SkippedProtectedFolder) {
        plan.steps = {
            Step::AppendSkippedProtectedProjectConsole,
            Step::RefreshTherionConfigDisplay,
            Step::UpdateProjectActionState,
            Step::RestoreOpenDocuments};
        return plan;
    }

    plan.steps = {
        Step::RefreshTherionConfigDisplay,
        Step::UpdateProjectActionState,
        Step::RestoreOpenDocuments};
    return plan;
}
}
