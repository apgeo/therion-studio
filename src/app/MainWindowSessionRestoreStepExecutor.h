#pragma once

#include "MainWindowSessionRestoreOrchestrationService.h"

#include <functional>
#include <QString>

namespace TherionStudio
{
class MainWindowSessionRestoreStepExecutor final
{
public:
    struct Actions
    {
        std::function<void(const QString &)> applyProjectRootToBrowser;
        std::function<void(const QString &)> appendRestoredProjectRootConsole;
        std::function<void()> loadStructureNameOverrides;
        std::function<void()> syncOpenDocumentsToProjectRoot;
        std::function<void()> rebuildStructureSidebar;
        std::function<void(const QString &)> appendSkippedProtectedProjectConsole;
        std::function<void()> refreshTherionConfigDisplay;
        std::function<void()> updateProjectActionState;
        std::function<void()> restoreOpenDocuments;
    };

    static void execute(const MainWindowSessionRestoreOrchestrationService::Plan &plan,
                        const Actions &actions);
};
}
