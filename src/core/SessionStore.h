#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
class SessionStore final
{
public:
    static QString lastProjectPath();
    static void setLastProjectPath(const QString &projectPath);

    static QByteArray mainWindowGeometry();
    static void setMainWindowGeometry(const QByteArray &geometry);

    static QByteArray mainWindowState();
    static void setMainWindowState(const QByteArray &state);

    static QStringList openDocumentPaths();
    static void setOpenDocumentPaths(const QStringList &documentPaths);

    static QString activeDocumentPath();
    static void setActiveDocumentPath(const QString &documentPath);

    static QString structureNameOverrides();
    static void setStructureNameOverrides(const QString &json);

    static QString therionExecutablePath();
    static void setTherionExecutablePath(const QString &path);

    static QString therionWorkingDirectory();
    static void setTherionWorkingDirectory(const QString &path);

    static QString therionArguments();
    static void setTherionArguments(const QString &arguments);

    static QString therionMapWorkspaceMode();
    static void setTherionMapWorkspaceMode(const QString &mode);
    static bool therionMapTouchFriendlyControlsEnabled();
    static void setTherionMapTouchFriendlyControlsEnabled(bool enabled);

    static QString therionMapBackgroundLayers();
    static void setTherionMapBackgroundLayers(const QString &json);
};
}
