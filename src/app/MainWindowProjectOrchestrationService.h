#pragma once

#include <vector>

namespace TherionStudio
{
class MainWindowProjectOrchestrationService final
{
public:
    enum class OpenProjectStep
    {
        ApplyProjectRootToBrowser,
        LoadStructureNameOverrides,
        SyncOpenDocumentsToProjectRoot,
        PersistSessionLastProjectPath,
        RebuildStructureSidebar,
        RefreshTherionConfigDisplay,
        UpdateProjectActionState,
        EnsureWelcomeTab
    };

    enum class CloseProjectStep
    {
        ClearDocumentTabs,
        ResetProjectBrowser,
        PersistSessionLastProjectPath,
        PersistOpenDocuments,
        RebuildStructureSidebar,
        RefreshTherionConfigDisplay,
        UpdateProjectActionState
    };

    static std::vector<OpenProjectStep> buildOpenProjectSteps(bool shouldEnsureWelcomeTab);
    static std::vector<CloseProjectStep> buildCloseProjectSteps();
};
}
