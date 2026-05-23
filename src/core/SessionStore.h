#pragma once

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QStringList>

#include <memory>

namespace TherionStudio
{
class SessionSettingsStore final
{
public:
    SessionSettingsStore();
    explicit SessionSettingsStore(const QString &fileName, QSettings::Format format = QSettings::IniFormat);
    SessionSettingsStore(QSettings::Format format,
                         QSettings::Scope scope,
                         const QString &organization,
                         const QString &application);
    ~SessionSettingsStore();

    SessionSettingsStore(const SessionSettingsStore &) = delete;
    SessionSettingsStore &operator=(const SessionSettingsStore &) = delete;
    SessionSettingsStore(SessionSettingsStore &&) noexcept;
    SessionSettingsStore &operator=(SessionSettingsStore &&) noexcept;

    QString lastProjectPath() const;
    void setLastProjectPath(const QString &projectPath);

    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray &geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray &state);

    QStringList openDocumentPaths() const;
    void setOpenDocumentPaths(const QStringList &documentPaths);

    QString activeDocumentPath() const;
    void setActiveDocumentPath(const QString &documentPath);

    QString structureNameOverrides() const;
    void setStructureNameOverrides(const QString &json);

    QString therionExecutablePath() const;
    void setTherionExecutablePath(const QString &path);

    QString therionWorkingDirectory() const;
    void setTherionWorkingDirectory(const QString &path);

    QString therionArguments() const;
    void setTherionArguments(const QString &arguments);

    QString therionRunTargetMode() const;
    void setTherionRunTargetMode(const QString &mode);

    QString therionTargetConfigPath() const;
    void setTherionTargetConfigPath(const QString &path);

    bool therionMapTouchFriendlyControlsEnabled() const;
    void setTherionMapTouchFriendlyControlsEnabled(bool enabled);

    QString therionMapBackgroundLayers() const;
    void setTherionMapBackgroundLayers(const QString &json);

private:
    std::unique_ptr<QSettings> settings_;
};

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

    static QString therionRunTargetMode();
    static void setTherionRunTargetMode(const QString &mode);

    static QString therionTargetConfigPath();
    static void setTherionTargetConfigPath(const QString &path);

    static bool therionMapTouchFriendlyControlsEnabled();
    static void setTherionMapTouchFriendlyControlsEnabled(bool enabled);

    static QString therionMapBackgroundLayers();
    static void setTherionMapBackgroundLayers(const QString &json);
};
}
