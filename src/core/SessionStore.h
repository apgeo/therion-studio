#pragma once

#include "ISessionStore.h"

#include <QSettings>

#include <memory>

namespace TherionStudio
{
class SessionSettingsStore final : public ISessionStore
{
public:
    SessionSettingsStore();
    explicit SessionSettingsStore(const QString &fileName, QSettings::Format format = QSettings::IniFormat);
    SessionSettingsStore(QSettings::Format format,
                         QSettings::Scope scope,
                         const QString &organization,
                         const QString &application);
    ~SessionSettingsStore() override;

    SessionSettingsStore(const SessionSettingsStore &) = delete;
    SessionSettingsStore &operator=(const SessionSettingsStore &) = delete;
    SessionSettingsStore(SessionSettingsStore &&) noexcept;
    SessionSettingsStore &operator=(SessionSettingsStore &&) noexcept;

    QString lastProjectPath() const override;
    void setLastProjectPath(const QString &projectPath) override;

    QByteArray mainWindowGeometry() const override;
    void setMainWindowGeometry(const QByteArray &geometry) override;

    QByteArray mainWindowState() const override;
    void setMainWindowState(const QByteArray &state) override;

    QStringList openDocumentPaths() const override;
    void setOpenDocumentPaths(const QStringList &documentPaths) override;

    QString activeDocumentPath() const override;
    void setActiveDocumentPath(const QString &documentPath) override;

    QString structureNameOverrides() const override;
    void setStructureNameOverrides(const QString &json) override;

    QString therionExecutablePath() const override;
    void setTherionExecutablePath(const QString &path) override;

    QString therionWorkingDirectory() const override;
    void setTherionWorkingDirectory(const QString &path) override;

    QString therionArguments() const override;
    void setTherionArguments(const QString &arguments) override;

    QString therionRunTargetMode() const override;
    void setTherionRunTargetMode(const QString &mode) override;

    QString therionTargetConfigPath() const override;
    void setTherionTargetConfigPath(const QString &path) override;

    bool therionMapTouchFriendlyControlsEnabled() const override;
    void setTherionMapTouchFriendlyControlsEnabled(bool enabled) override;

    bool therionMapMagnifierEnabled() const override;
    void setTherionMapMagnifierEnabled(bool enabled) override;

    QString therionMapBackgroundLayers() const override;
    void setTherionMapBackgroundLayers(const QString &json) override;

private:
    std::unique_ptr<QSettings> settings_;
};

class InMemorySessionStore final : public ISessionStore
{
public:
    QString lastProjectPath() const override;
    void setLastProjectPath(const QString &projectPath) override;

    QByteArray mainWindowGeometry() const override;
    void setMainWindowGeometry(const QByteArray &geometry) override;

    QByteArray mainWindowState() const override;
    void setMainWindowState(const QByteArray &state) override;

    QStringList openDocumentPaths() const override;
    void setOpenDocumentPaths(const QStringList &documentPaths) override;

    QString activeDocumentPath() const override;
    void setActiveDocumentPath(const QString &documentPath) override;

    QString structureNameOverrides() const override;
    void setStructureNameOverrides(const QString &json) override;

    QString therionExecutablePath() const override;
    void setTherionExecutablePath(const QString &path) override;

    QString therionWorkingDirectory() const override;
    void setTherionWorkingDirectory(const QString &path) override;

    QString therionArguments() const override;
    void setTherionArguments(const QString &arguments) override;

    QString therionRunTargetMode() const override;
    void setTherionRunTargetMode(const QString &mode) override;

    QString therionTargetConfigPath() const override;
    void setTherionTargetConfigPath(const QString &path) override;

    bool therionMapTouchFriendlyControlsEnabled() const override;
    void setTherionMapTouchFriendlyControlsEnabled(bool enabled) override;

    bool therionMapMagnifierEnabled() const override;
    void setTherionMapMagnifierEnabled(bool enabled) override;

    QString therionMapBackgroundLayers() const override;
    void setTherionMapBackgroundLayers(const QString &json) override;

private:
    QString lastProjectPath_;
    QByteArray mainWindowGeometry_;
    QByteArray mainWindowState_;
    QStringList openDocumentPaths_;
    QString activeDocumentPath_;
    QString structureNameOverrides_;
    QString therionExecutablePath_;
    QString therionWorkingDirectory_;
    QString therionArguments_;
    QString therionRunTargetMode_ = QStringLiteral("project");
    QString therionTargetConfigPath_;
    bool therionMapTouchFriendlyControlsEnabled_ = false;
    bool therionMapMagnifierEnabled_ = true;
    QString therionMapBackgroundLayers_;
};
}
