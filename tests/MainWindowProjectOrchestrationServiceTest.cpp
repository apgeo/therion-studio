#include "../src/app/MainWindowProjectOrchestrationService.h"

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

int runOpenProjectStepsTest()
{
    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> withWelcome =
        MainWindowProjectOrchestrationService::buildOpenProjectSteps(true);
    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> expectedWithWelcome = {
        MainWindowProjectOrchestrationService::OpenProjectStep::ApplyProjectRootToBrowser,
        MainWindowProjectOrchestrationService::OpenProjectStep::LoadStructureNameOverrides,
        MainWindowProjectOrchestrationService::OpenProjectStep::SyncOpenDocumentsToProjectRoot,
        MainWindowProjectOrchestrationService::OpenProjectStep::PersistSessionLastProjectPath,
        MainWindowProjectOrchestrationService::OpenProjectStep::RebuildStructureSidebar,
        MainWindowProjectOrchestrationService::OpenProjectStep::RefreshTherionConfigDisplay,
        MainWindowProjectOrchestrationService::OpenProjectStep::UpdateProjectActionState,
        MainWindowProjectOrchestrationService::OpenProjectStep::EnsureWelcomeTab};
    if (!expect(withWelcome == expectedWithWelcome,
                "Open-project orchestration steps with welcome tab changed unexpectedly.")) {
        return 1;
    }

    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> withoutWelcome =
        MainWindowProjectOrchestrationService::buildOpenProjectSteps(false);
    const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> expectedWithoutWelcome = {
        MainWindowProjectOrchestrationService::OpenProjectStep::ApplyProjectRootToBrowser,
        MainWindowProjectOrchestrationService::OpenProjectStep::LoadStructureNameOverrides,
        MainWindowProjectOrchestrationService::OpenProjectStep::SyncOpenDocumentsToProjectRoot,
        MainWindowProjectOrchestrationService::OpenProjectStep::PersistSessionLastProjectPath,
        MainWindowProjectOrchestrationService::OpenProjectStep::RebuildStructureSidebar,
        MainWindowProjectOrchestrationService::OpenProjectStep::RefreshTherionConfigDisplay,
        MainWindowProjectOrchestrationService::OpenProjectStep::UpdateProjectActionState};
    if (!expect(withoutWelcome == expectedWithoutWelcome,
                "Open-project orchestration steps without welcome tab changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectStepsTest()
{
    const std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> steps =
        MainWindowProjectOrchestrationService::buildCloseProjectSteps();
    const std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> expected = {
        MainWindowProjectOrchestrationService::CloseProjectStep::ClearDocumentTabs,
        MainWindowProjectOrchestrationService::CloseProjectStep::ResetProjectBrowser,
        MainWindowProjectOrchestrationService::CloseProjectStep::PersistSessionLastProjectPath,
        MainWindowProjectOrchestrationService::CloseProjectStep::PersistOpenDocuments,
        MainWindowProjectOrchestrationService::CloseProjectStep::RebuildStructureSidebar,
        MainWindowProjectOrchestrationService::CloseProjectStep::RefreshTherionConfigDisplay,
        MainWindowProjectOrchestrationService::CloseProjectStep::UpdateProjectActionState};
    if (!expect(steps == expected,
                "Close-project orchestration steps changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runOpenProjectStepsTest() != 0) {
        return 1;
    }
    if (runCloseProjectStepsTest() != 0) {
        return 1;
    }

    return 0;
}
