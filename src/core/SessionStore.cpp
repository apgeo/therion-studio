#include "SessionStore.h"

#include <QSettings>

#include <memory>

namespace TherionStudio
{
namespace
{
const auto kLastProjectPathKey = QStringLiteral("session/lastProjectPath");
const auto kMainWindowGeometryKey = QStringLiteral("session/mainWindowGeometry");
const auto kMainWindowStateKey = QStringLiteral("session/mainWindowState");
const auto kOpenDocumentPathsKey = QStringLiteral("session/openDocumentPaths");
const auto kActiveDocumentPathKey = QStringLiteral("session/activeDocumentPath");
const auto kStructureNameOverridesKey = QStringLiteral("session/structureNameOverrides");
const auto kTherionExecutablePathKey = QStringLiteral("session/therionExecutablePath");
const auto kTherionWorkingDirectoryKey = QStringLiteral("session/therionWorkingDirectory");
const auto kTherionArgumentsKey = QStringLiteral("session/therionArguments");
const auto kTherionRunTargetModeKey = QStringLiteral("session/therionRunTargetMode");
const auto kTherionTargetConfigPathKey = QStringLiteral("session/therionTargetConfigPath");
const auto kTherionMapTouchFriendlyControlsEnabledKey = QStringLiteral("session/therionMapTouchFriendlyControlsEnabled");
const auto kTherionMapBackgroundLayersKey = QStringLiteral("session/therionMapBackgroundLayers");
}

SessionSettingsStore::SessionSettingsStore()
    : settings_(std::make_unique<QSettings>())
{
}

SessionSettingsStore::SessionSettingsStore(const QString &fileName, QSettings::Format format)
    : settings_(std::make_unique<QSettings>(fileName, format))
{
}

SessionSettingsStore::SessionSettingsStore(QSettings::Format format,
                                           QSettings::Scope scope,
                                           const QString &organization,
                                           const QString &application)
    : settings_(std::make_unique<QSettings>(format, scope, organization, application))
{
}

SessionSettingsStore::~SessionSettingsStore() = default;

SessionSettingsStore::SessionSettingsStore(SessionSettingsStore &&) noexcept = default;

SessionSettingsStore &SessionSettingsStore::operator=(SessionSettingsStore &&) noexcept = default;

QString SessionSettingsStore::lastProjectPath() const
{
    return settings_->value(kLastProjectPathKey).toString();
}

void SessionSettingsStore::setLastProjectPath(const QString &projectPath)
{
    settings_->setValue(kLastProjectPathKey, projectPath);
}

QByteArray SessionSettingsStore::mainWindowGeometry() const
{
    return settings_->value(kMainWindowGeometryKey).toByteArray();
}

void SessionSettingsStore::setMainWindowGeometry(const QByteArray &geometry)
{
    settings_->setValue(kMainWindowGeometryKey, geometry);
}

QByteArray SessionSettingsStore::mainWindowState() const
{
    return settings_->value(kMainWindowStateKey).toByteArray();
}

void SessionSettingsStore::setMainWindowState(const QByteArray &state)
{
    settings_->setValue(kMainWindowStateKey, state);
}

QStringList SessionSettingsStore::openDocumentPaths() const
{
    return settings_->value(kOpenDocumentPathsKey).toStringList();
}

void SessionSettingsStore::setOpenDocumentPaths(const QStringList &documentPaths)
{
    settings_->setValue(kOpenDocumentPathsKey, documentPaths);
}

QString SessionSettingsStore::activeDocumentPath() const
{
    return settings_->value(kActiveDocumentPathKey).toString();
}

void SessionSettingsStore::setActiveDocumentPath(const QString &documentPath)
{
    settings_->setValue(kActiveDocumentPathKey, documentPath);
}

QString SessionSettingsStore::structureNameOverrides() const
{
    return settings_->value(kStructureNameOverridesKey).toString();
}

void SessionSettingsStore::setStructureNameOverrides(const QString &json)
{
    settings_->setValue(kStructureNameOverridesKey, json);
}

QString SessionSettingsStore::therionExecutablePath() const
{
    return settings_->value(kTherionExecutablePathKey).toString();
}

void SessionSettingsStore::setTherionExecutablePath(const QString &path)
{
    settings_->setValue(kTherionExecutablePathKey, path);
}

QString SessionSettingsStore::therionWorkingDirectory() const
{
    return settings_->value(kTherionWorkingDirectoryKey).toString();
}

void SessionSettingsStore::setTherionWorkingDirectory(const QString &path)
{
    settings_->setValue(kTherionWorkingDirectoryKey, path);
}

QString SessionSettingsStore::therionArguments() const
{
    return settings_->value(kTherionArgumentsKey).toString();
}

void SessionSettingsStore::setTherionArguments(const QString &arguments)
{
    settings_->setValue(kTherionArgumentsKey, arguments);
}

QString SessionSettingsStore::therionRunTargetMode() const
{
    return settings_->value(kTherionRunTargetModeKey, QStringLiteral("project")).toString();
}

void SessionSettingsStore::setTherionRunTargetMode(const QString &mode)
{
    settings_->setValue(kTherionRunTargetModeKey, mode);
}

QString SessionSettingsStore::therionTargetConfigPath() const
{
    return settings_->value(kTherionTargetConfigPathKey).toString();
}

void SessionSettingsStore::setTherionTargetConfigPath(const QString &path)
{
    settings_->setValue(kTherionTargetConfigPathKey, path);
}

bool SessionSettingsStore::therionMapTouchFriendlyControlsEnabled() const
{
    return settings_->value(kTherionMapTouchFriendlyControlsEnabledKey, false).toBool();
}

void SessionSettingsStore::setTherionMapTouchFriendlyControlsEnabled(bool enabled)
{
    settings_->setValue(kTherionMapTouchFriendlyControlsEnabledKey, enabled);
}

QString SessionSettingsStore::therionMapBackgroundLayers() const
{
    return settings_->value(kTherionMapBackgroundLayersKey).toString();
}

void SessionSettingsStore::setTherionMapBackgroundLayers(const QString &json)
{
    settings_->setValue(kTherionMapBackgroundLayersKey, json);
}

QString SessionStore::lastProjectPath()
{
    return SessionSettingsStore().lastProjectPath();
}

void SessionStore::setLastProjectPath(const QString &projectPath)
{
    SessionSettingsStore().setLastProjectPath(projectPath);
}

QByteArray SessionStore::mainWindowGeometry()
{
    return SessionSettingsStore().mainWindowGeometry();
}

void SessionStore::setMainWindowGeometry(const QByteArray &geometry)
{
    SessionSettingsStore().setMainWindowGeometry(geometry);
}

QByteArray SessionStore::mainWindowState()
{
    return SessionSettingsStore().mainWindowState();
}

void SessionStore::setMainWindowState(const QByteArray &state)
{
    SessionSettingsStore().setMainWindowState(state);
}

QStringList SessionStore::openDocumentPaths()
{
    return SessionSettingsStore().openDocumentPaths();
}

void SessionStore::setOpenDocumentPaths(const QStringList &documentPaths)
{
    SessionSettingsStore().setOpenDocumentPaths(documentPaths);
}

QString SessionStore::activeDocumentPath()
{
    return SessionSettingsStore().activeDocumentPath();
}

void SessionStore::setActiveDocumentPath(const QString &documentPath)
{
    SessionSettingsStore().setActiveDocumentPath(documentPath);
}

QString SessionStore::structureNameOverrides()
{
    return SessionSettingsStore().structureNameOverrides();
}

void SessionStore::setStructureNameOverrides(const QString &json)
{
    SessionSettingsStore().setStructureNameOverrides(json);
}

QString SessionStore::therionExecutablePath()
{
    return SessionSettingsStore().therionExecutablePath();
}

void SessionStore::setTherionExecutablePath(const QString &path)
{
    SessionSettingsStore().setTherionExecutablePath(path);
}

QString SessionStore::therionWorkingDirectory()
{
    return SessionSettingsStore().therionWorkingDirectory();
}

void SessionStore::setTherionWorkingDirectory(const QString &path)
{
    SessionSettingsStore().setTherionWorkingDirectory(path);
}

QString SessionStore::therionArguments()
{
    return SessionSettingsStore().therionArguments();
}

void SessionStore::setTherionArguments(const QString &arguments)
{
    SessionSettingsStore().setTherionArguments(arguments);
}

QString SessionStore::therionRunTargetMode()
{
    return SessionSettingsStore().therionRunTargetMode();
}

void SessionStore::setTherionRunTargetMode(const QString &mode)
{
    SessionSettingsStore().setTherionRunTargetMode(mode);
}

QString SessionStore::therionTargetConfigPath()
{
    return SessionSettingsStore().therionTargetConfigPath();
}

void SessionStore::setTherionTargetConfigPath(const QString &path)
{
    SessionSettingsStore().setTherionTargetConfigPath(path);
}

bool SessionStore::therionMapTouchFriendlyControlsEnabled()
{
    return SessionSettingsStore().therionMapTouchFriendlyControlsEnabled();
}

void SessionStore::setTherionMapTouchFriendlyControlsEnabled(bool enabled)
{
    SessionSettingsStore().setTherionMapTouchFriendlyControlsEnabled(enabled);
}

QString SessionStore::therionMapBackgroundLayers()
{
    return SessionSettingsStore().therionMapBackgroundLayers();
}

void SessionStore::setTherionMapBackgroundLayers(const QString &json)
{
    SessionSettingsStore().setTherionMapBackgroundLayers(json);
}
}
