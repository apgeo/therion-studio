#include "SessionStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace TherionStudio
{
namespace
{
QSettings makeSettings()
{
    return QSettings();
}
}

QString SessionStore::lastProjectPath()
{
    return makeSettings().value(QStringLiteral("session/lastProjectPath")).toString();
}

void SessionStore::setLastProjectPath(const QString &projectPath)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/lastProjectPath"), projectPath);
}

QByteArray SessionStore::mainWindowGeometry()
{
    return makeSettings().value(QStringLiteral("session/mainWindowGeometry")).toByteArray();
}

void SessionStore::setMainWindowGeometry(const QByteArray &geometry)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/mainWindowGeometry"), geometry);
}

QByteArray SessionStore::mainWindowState()
{
    return makeSettings().value(QStringLiteral("session/mainWindowState")).toByteArray();
}

void SessionStore::setMainWindowState(const QByteArray &state)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/mainWindowState"), state);
}

QStringList SessionStore::openDocumentPaths()
{
    return makeSettings().value(QStringLiteral("session/openDocumentPaths")).toStringList();
}

void SessionStore::setOpenDocumentPaths(const QStringList &documentPaths)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/openDocumentPaths"), documentPaths);
}

QString SessionStore::activeDocumentPath()
{
    return makeSettings().value(QStringLiteral("session/activeDocumentPath")).toString();
}

void SessionStore::setActiveDocumentPath(const QString &documentPath)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/activeDocumentPath"), documentPath);
}

QString SessionStore::structureNameOverrides()
{
    return makeSettings().value(QStringLiteral("session/structureNameOverrides")).toString();
}

void SessionStore::setStructureNameOverrides(const QString &json)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/structureNameOverrides"), json);
}

QString SessionStore::therionExecutablePath()
{
    return makeSettings().value(QStringLiteral("session/therionExecutablePath")).toString();
}

void SessionStore::setTherionExecutablePath(const QString &path)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/therionExecutablePath"), path);
}

QString SessionStore::therionWorkingDirectory()
{
    return makeSettings().value(QStringLiteral("session/therionWorkingDirectory")).toString();
}

void SessionStore::setTherionWorkingDirectory(const QString &path)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/therionWorkingDirectory"), path);
}

QString SessionStore::therionArguments()
{
    return makeSettings().value(QStringLiteral("session/therionArguments")).toString();
}

void SessionStore::setTherionArguments(const QString &arguments)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/therionArguments"), arguments);
}

QString SessionStore::therionMapWorkspaceMode()
{
    return makeSettings().value(QStringLiteral("session/therionMapWorkspaceMode")).toString();
}

void SessionStore::setTherionMapWorkspaceMode(const QString &mode)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/therionMapWorkspaceMode"), mode);
}

bool SessionStore::therionMapTouchFriendlyControlsEnabled()
{
    return makeSettings().value(QStringLiteral("session/therionMapTouchFriendlyControlsEnabled"), false).toBool();
}

void SessionStore::setTherionMapTouchFriendlyControlsEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/therionMapTouchFriendlyControlsEnabled"), enabled);
}

QString SessionStore::therionMapBackgroundLayers()
{
    return makeSettings().value(QStringLiteral("session/therionMapBackgroundLayers")).toString();
}

void SessionStore::setTherionMapBackgroundLayers(const QString &json)
{
    QSettings settings;
    settings.setValue(QStringLiteral("session/therionMapBackgroundLayers"), json);
}
}
