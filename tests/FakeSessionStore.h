#pragma once

#include "../src/core/ISessionStore.h"

namespace TherionStudio
{
class FakeSessionStore final : public ISessionStore
{
public:
    QString lastProjectPath() const override { return lastProjectPath_; }
    void setLastProjectPath(const QString &projectPath) override { lastProjectPath_ = projectPath; }

    QByteArray mainWindowGeometry() const override { return mainWindowGeometry_; }
    void setMainWindowGeometry(const QByteArray &geometry) override { mainWindowGeometry_ = geometry; }

    QByteArray mainWindowState() const override { return mainWindowState_; }
    void setMainWindowState(const QByteArray &state) override { mainWindowState_ = state; }

    QStringList openDocumentPaths() const override { return openDocumentPaths_; }
    void setOpenDocumentPaths(const QStringList &documentPaths) override { openDocumentPaths_ = documentPaths; }

    QString activeDocumentPath() const override { return activeDocumentPath_; }
    void setActiveDocumentPath(const QString &documentPath) override { activeDocumentPath_ = documentPath; }

    QString structureNameOverrides() const override { return structureNameOverrides_; }
    void setStructureNameOverrides(const QString &json) override { structureNameOverrides_ = json; }

    QString therionExecutablePath() const override { return therionExecutablePath_; }
    void setTherionExecutablePath(const QString &path) override { therionExecutablePath_ = path; }

    QString therionWorkingDirectory() const override { return therionWorkingDirectory_; }
    void setTherionWorkingDirectory(const QString &path) override { therionWorkingDirectory_ = path; }

    QString therionArguments() const override { return therionArguments_; }
    void setTherionArguments(const QString &arguments) override { therionArguments_ = arguments; }

    QString therionRunTargetMode() const override { return therionRunTargetMode_; }
    void setTherionRunTargetMode(const QString &mode) override { therionRunTargetMode_ = mode; }

    QString therionTargetConfigPath() const override { return therionTargetConfigPath_; }
    void setTherionTargetConfigPath(const QString &path) override { therionTargetConfigPath_ = path; }

    bool therionMapTouchFriendlyControlsEnabled() const override { return therionMapTouchFriendlyControlsEnabled_; }
    void setTherionMapTouchFriendlyControlsEnabled(bool enabled) override { therionMapTouchFriendlyControlsEnabled_ = enabled; }

    QString therionMapBackgroundLayers() const override { return therionMapBackgroundLayers_; }
    void setTherionMapBackgroundLayers(const QString &json) override { therionMapBackgroundLayers_ = json; }

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
    QString therionRunTargetMode_;
    QString therionTargetConfigPath_;
    bool therionMapTouchFriendlyControlsEnabled_ = false;
    QString therionMapBackgroundLayers_;
};
}
