#pragma once

#include "../core/ISessionStore.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
class MainWindowSessionStateService final
{
public:
    struct MainWindowStateSnapshot
    {
        QByteArray windowGeometry;
        QByteArray windowState;
        QString projectRootPath;
        QString therionExecutablePath;
        QString therionWorkingDirectory;
        QString therionArguments;
        QString therionRunTargetMode;
        QString therionTargetConfigPath;
        QString structureNameOverridesJson;
    };

    struct OpenDocumentsSnapshot
    {
        QStringList openDocumentPaths;
        QString activeDocumentPath;
    };

    static void persistMainWindowState(ISessionStore &sessionStore,
                                       const MainWindowStateSnapshot &snapshot);
    static void persistOpenDocuments(ISessionStore &sessionStore,
                                     const OpenDocumentsSnapshot &snapshot);
};
}
