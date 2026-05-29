#include "MainWindowSessionStateService.h"

namespace TherionStudio
{
void MainWindowSessionStateService::persistMainWindowState(ISessionStore &sessionStore,
                                                           const MainWindowStateSnapshot &snapshot)
{
    sessionStore.setMainWindowGeometry(snapshot.windowGeometry);
    sessionStore.setMainWindowState(snapshot.windowState);
    sessionStore.setLastProjectPath(snapshot.projectRootPath);
    sessionStore.setTherionExecutablePath(snapshot.therionExecutablePath);
    sessionStore.setTherionWorkingDirectory(snapshot.therionWorkingDirectory);
    sessionStore.setTherionArguments(snapshot.therionArguments);
    sessionStore.setTherionRunTargetMode(snapshot.therionRunTargetMode);
    sessionStore.setTherionTargetConfigPath(snapshot.therionTargetConfigPath);
    sessionStore.setStructureNameOverrides(snapshot.structureNameOverridesJson);
}

void MainWindowSessionStateService::persistOpenDocuments(ISessionStore &sessionStore,
                                                         const OpenDocumentsSnapshot &snapshot)
{
    sessionStore.setOpenDocumentPaths(snapshot.openDocumentPaths);
    sessionStore.setActiveDocumentPath(snapshot.activeDocumentPath);
}
}
