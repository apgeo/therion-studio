#include "../src/app/MainWindowProjectWorkspaceService.h"

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

int runOpenProjectWorkspaceStateTest()
{
    const auto noTabs = MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
        QStringLiteral("/tmp/project"), true, false);
    if (!expect(noTabs.projectRootPath == QStringLiteral("/tmp/project"),
                "Open-project workspace state should preserve project root path.")) {
        return 1;
    }
    if (!expect(noTabs.sessionLastProjectPath == QStringLiteral("/tmp/project"),
                "Open-project workspace state should persist the same session project path.")) {
        return 1;
    }
    if (!expect(noTabs.shouldEnsureWelcomeTab,
                "Open-project workspace state should request welcome tab when no tabs exist.")) {
        return 1;
    }

    const auto hasWelcome = MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
        QStringLiteral("/tmp/project"), false, true);
    if (!expect(hasWelcome.shouldEnsureWelcomeTab,
                "Open-project workspace state should request welcome tab when welcome tab already exists.")) {
        return 1;
    }

    const auto hasRegularTabs = MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
        QStringLiteral("/tmp/project"), false, false);
    if (!expect(!hasRegularTabs.shouldEnsureWelcomeTab,
                "Open-project workspace state should not request welcome tab when regular tabs are present.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectWorkspaceStateTest()
{
    const auto close = MainWindowProjectWorkspaceService::buildCloseProjectWorkspaceState(
        QStringLiteral("/tmp/project"));
    if (!expect(close.projectRootPath.isEmpty(),
                "Close-project workspace state should clear current project root path.")) {
        return 1;
    }
    if (!expect(close.sessionLastProjectPath.isEmpty(),
                "Close-project workspace state should clear session project path.")) {
        return 1;
    }
    if (!expect(close.closedProjectPath == QStringLiteral("/tmp/project"),
                "Close-project workspace state should preserve closed path for logging.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runOpenProjectWorkspaceStateTest() != 0) {
        return 1;
    }
    if (runCloseProjectWorkspaceStateTest() != 0) {
        return 1;
    }

    return 0;
}
