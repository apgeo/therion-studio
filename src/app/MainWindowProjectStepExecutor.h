#pragma once

#include "MainWindowProjectOrchestrationService.h"

#include <functional>
#include <vector>

namespace TherionStudio
{
class MainWindowProjectStepExecutor final
{
public:
    struct OpenProjectActions
    {
        std::function<void()> applyProjectRootToBrowser;
        std::function<void()> loadStructureNameOverrides;
        std::function<void()> syncOpenDocumentsToProjectRoot;
        std::function<void()> persistSessionLastProjectPath;
        std::function<void()> rebuildStructureSidebar;
        std::function<void()> refreshTherionConfigDisplay;
        std::function<void()> updateProjectActionState;
        std::function<void()> ensureWelcomeTab;
    };

    struct CloseProjectActions
    {
        std::function<void()> clearDocumentTabs;
        std::function<void()> resetProjectBrowser;
        std::function<void()> persistSessionLastProjectPath;
        std::function<void()> persistOpenDocuments;
        std::function<void()> rebuildStructureSidebar;
        std::function<void()> refreshTherionConfigDisplay;
        std::function<void()> updateProjectActionState;
    };

    static void executeOpenProjectSteps(const std::vector<MainWindowProjectOrchestrationService::OpenProjectStep> &steps,
                                        const OpenProjectActions &actions);
    static void executeCloseProjectSteps(const std::vector<MainWindowProjectOrchestrationService::CloseProjectStep> &steps,
                                         const CloseProjectActions &actions);
};
}
