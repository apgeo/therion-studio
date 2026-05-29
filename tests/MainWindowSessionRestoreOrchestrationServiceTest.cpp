#include "../src/app/MainWindowSessionRestoreOrchestrationService.h"

#include <vector>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

MainWindowSessionProjectService::ProjectRestoreDecision makeDecision(MainWindowSessionProjectService::ProjectRestoreStatus status,
                                                                     const QString &path)
{
    MainWindowSessionProjectService::ProjectRestoreDecision decision;
    decision.status = status;
    decision.projectPath = path;
    return decision;
}

int runRestoredPlanTest()
{
    const auto plan = MainWindowSessionRestoreOrchestrationService::buildPlan(
        makeDecision(MainWindowSessionProjectService::ProjectRestoreStatus::Restored,
                     QStringLiteral("/tmp/project")));

    const std::vector<MainWindowSessionRestoreOrchestrationService::Step> expected = {
        MainWindowSessionRestoreOrchestrationService::Step::ApplyProjectRootToBrowser,
        MainWindowSessionRestoreOrchestrationService::Step::AppendRestoredProjectRootConsole,
        MainWindowSessionRestoreOrchestrationService::Step::LoadStructureNameOverrides,
        MainWindowSessionRestoreOrchestrationService::Step::SyncOpenDocumentsToProjectRoot,
        MainWindowSessionRestoreOrchestrationService::Step::RebuildStructureSidebar,
        MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay,
        MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState,
        MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments};

    if (!expect(plan.steps == expected,
                "Restored-project session restore plan steps changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runSkippedProtectedPlanTest()
{
    const auto plan = MainWindowSessionRestoreOrchestrationService::buildPlan(
        makeDecision(MainWindowSessionProjectService::ProjectRestoreStatus::SkippedProtectedFolder,
                     QStringLiteral("/tmp/protected")));

    const std::vector<MainWindowSessionRestoreOrchestrationService::Step> expected = {
        MainWindowSessionRestoreOrchestrationService::Step::AppendSkippedProtectedProjectConsole,
        MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay,
        MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState,
        MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments};

    if (!expect(plan.steps == expected,
                "Skipped-protected session restore plan steps changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runNoProjectPlanTest()
{
    const auto plan = MainWindowSessionRestoreOrchestrationService::buildPlan(
        makeDecision(MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored,
                     QString()));

    const std::vector<MainWindowSessionRestoreOrchestrationService::Step> expected = {
        MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay,
        MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState,
        MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments};

    if (!expect(plan.steps == expected,
                "No-project session restore plan steps changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runRestoredPlanTest() != 0) {
        return 1;
    }
    if (runSkippedProtectedPlanTest() != 0) {
        return 1;
    }
    if (runNoProjectPlanTest() != 0) {
        return 1;
    }

    return 0;
}
