#include "../src/app/MainWindowSessionStateService.h"
#include "FakeSessionStore.h"

#include <QByteArray>
#include <QStringList>

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

int runMainWindowStatePersistTest()
{
    FakeSessionStore sessionStore;

    MainWindowSessionStateService::MainWindowStateSnapshot snapshot;
    snapshot.windowGeometry = QByteArrayLiteral("geometry");
    snapshot.windowState = QByteArrayLiteral("state");
    snapshot.projectRootPath = QStringLiteral("/tmp/project");
    snapshot.therionExecutablePath = QStringLiteral("/opt/therion");
    snapshot.therionWorkingDirectory = QStringLiteral("/tmp/project/work");
    snapshot.therionArguments = QStringLiteral("--config thconfig");
    snapshot.therionRunTargetMode = QStringLiteral("current");
    snapshot.therionTargetConfigPath = QStringLiteral("/tmp/project/thconfig");
    snapshot.structureNameOverridesJson = QStringLiteral("{\"a\":\"b\"}");

    MainWindowSessionStateService::persistMainWindowState(sessionStore, snapshot);

    if (!expect(sessionStore.mainWindowGeometry() == snapshot.windowGeometry,
                "Window geometry should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.mainWindowState() == snapshot.windowState,
                "Window state should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.lastProjectPath() == snapshot.projectRootPath,
                "Project path should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.therionExecutablePath() == snapshot.therionExecutablePath,
                "Therion executable path should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.therionWorkingDirectory() == snapshot.therionWorkingDirectory,
                "Therion working directory should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.therionArguments() == snapshot.therionArguments,
                "Therion arguments should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.therionRunTargetMode() == snapshot.therionRunTargetMode,
                "Therion run target mode should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.therionTargetConfigPath() == snapshot.therionTargetConfigPath,
                "Therion target config path should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.structureNameOverrides() == snapshot.structureNameOverridesJson,
                "Structure name overrides JSON should persist into session store.")) {
        return 1;
    }

    return 0;
}

int runOpenDocumentsPersistTest()
{
    FakeSessionStore sessionStore;

    MainWindowSessionStateService::OpenDocumentsSnapshot snapshot;
    snapshot.openDocumentPaths = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th2")};
    snapshot.activeDocumentPath = QStringLiteral("/tmp/b.th2");

    MainWindowSessionStateService::persistOpenDocuments(sessionStore, snapshot);

    if (!expect(sessionStore.openDocumentPaths() == snapshot.openDocumentPaths,
                "Open document paths should persist into session store.")) {
        return 1;
    }
    if (!expect(sessionStore.activeDocumentPath() == snapshot.activeDocumentPath,
                "Active document path should persist into session store.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runMainWindowStatePersistTest() != 0) {
        return 1;
    }
    if (runOpenDocumentsPersistTest() != 0) {
        return 1;
    }

    return 0;
}
