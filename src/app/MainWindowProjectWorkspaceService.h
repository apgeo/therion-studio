#pragma once

#include <QString>

namespace TherionStudio
{
class MainWindowProjectWorkspaceService final
{
public:
    struct OpenProjectWorkspaceState
    {
        QString projectRootPath;
        QString sessionLastProjectPath;
        bool shouldEnsureWelcomeTab = false;
    };

    struct CloseProjectWorkspaceState
    {
        QString projectRootPath;
        QString sessionLastProjectPath;
        QString closedProjectPath;
    };

    static OpenProjectWorkspaceState buildOpenProjectWorkspaceState(const QString &projectPath,
                                                                    bool hasNoTabs,
                                                                    bool hasWelcomeTab);
    static CloseProjectWorkspaceState buildCloseProjectWorkspaceState(const QString &currentProjectPath);
};
}
