#include "MainWindowProjectWorkspaceService.h"

namespace TherionStudio
{
MainWindowProjectWorkspaceService::OpenProjectWorkspaceState MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(const QString &projectPath,
                                                                                                                              bool hasNoTabs,
                                                                                                                              bool hasWelcomeTab)
{
    OpenProjectWorkspaceState state;
    state.projectRootPath = projectPath;
    state.sessionLastProjectPath = projectPath;
    state.shouldEnsureWelcomeTab = hasNoTabs || hasWelcomeTab;
    return state;
}

MainWindowProjectWorkspaceService::CloseProjectWorkspaceState MainWindowProjectWorkspaceService::buildCloseProjectWorkspaceState(const QString &currentProjectPath)
{
    CloseProjectWorkspaceState state;
    state.projectRootPath = QString();
    state.sessionLastProjectPath = QString();
    state.closedProjectPath = currentProjectPath;
    return state;
}
}
