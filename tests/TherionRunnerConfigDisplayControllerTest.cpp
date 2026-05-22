#include "../src/app/TherionRunnerConfigDisplayController.h"

#include <QDir>
#include <QTemporaryDir>

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

int runEmptyResolvedPathClearsAutoWorkingDirectoryTest()
{
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(QString(),
                                                      false,
                                                      QStringLiteral("/tmp/work"),
                                                      QStringLiteral("/tmp/project"),
                                                      QStringLiteral("/tmp/old-auto"));

    if (!expect(!result.hasResolvedConfigPath, "Empty config path should not produce a resolved config state.")) {
        return 1;
    }
    if (!expect(result.shouldClearAutoResolvedWorkingDirectory,
                "Empty config path should clear the auto-resolved working directory.")) {
        return 1;
    }
    if (!expect(!result.shouldUpdateWorkingDirectoryText,
                "Empty config path should not update the working-directory field.")) {
        return 1;
    }

    return 0;
}

int runExplicitConfigDoesNotAutoUpdateWorkingDirectoryTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("custom.thconfig"));
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(configPath,
                                                      true,
                                                      QString(),
                                                      tempDir.path(),
                                                      QString());

    if (!expect(result.hasResolvedConfigPath, "Explicit config path should produce a resolved config state.")) {
        return 1;
    }
    if (!expect(result.configName == QStringLiteral("custom.thconfig"),
                "Resolved config name should come from the config path.")) {
        return 1;
    }
    if (!expect(!result.shouldUpdateWorkingDirectoryText,
                "Explicit config arguments should not auto-update the working-directory field.")) {
        return 1;
    }
    if (!expect(!result.shouldUpdateAutoResolvedWorkingDirectory,
                "Explicit config arguments should not update the auto-resolved working-directory marker.")) {
        return 1;
    }

    return 0;
}

int runBlankWorkingDirectoryAutoUpdatesToConfigDirectoryTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("thconfig"));
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(configPath,
                                                      false,
                                                      QString(),
                                                      QString(),
                                                      QString());

    if (!expect(result.shouldUpdateWorkingDirectoryText,
                "Blank working-directory field should be auto-updated to the config directory.")) {
        return 1;
    }
    if (!expect(result.updatedWorkingDirectoryText == tempDir.path(),
                "Auto-updated working directory should match the config directory.")) {
        return 1;
    }
    if (!expect(result.shouldUpdateAutoResolvedWorkingDirectory
                    && result.updatedAutoResolvedWorkingDirectory == tempDir.path(),
                "Auto-resolved working-directory marker should track the config directory.")) {
        return 1;
    }

    return 0;
}

int runProjectRootWorkingDirectoryAutoUpdatesToConfigDirectoryTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    QDir root(tempDir.path());
    if (!expect(root.mkpath(QStringLiteral("project")) && root.mkpath(QStringLiteral("project/configs")),
                "Temporary project config directories could not be created.")) {
        return 1;
    }

    const QString projectRoot = root.filePath(QStringLiteral("project"));
    const QString configDirectory = root.filePath(QStringLiteral("project/configs"));
    const QString configPath = QDir(configDirectory).filePath(QStringLiteral("therion.thconfig"));
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(configPath,
                                                      false,
                                                      projectRoot,
                                                      projectRoot,
                                                      QString());

    if (!expect(result.shouldUpdateWorkingDirectoryText,
                "Project-root working directory should auto-update to a discovered config directory.")) {
        return 1;
    }
    if (!expect(result.updatedWorkingDirectoryText == configDirectory,
                "Project-root auto-update should point at the config directory.")) {
        return 1;
    }

    return 0;
}

int runManualWorkingDirectoryIsPreservedTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    QDir root(tempDir.path());
    if (!expect(root.mkpath(QStringLiteral("manual")) && root.mkpath(QStringLiteral("config")),
                "Temporary manual/config directories could not be created.")) {
        return 1;
    }

    const QString manualDirectory = root.filePath(QStringLiteral("manual"));
    const QString configDirectory = root.filePath(QStringLiteral("config"));
    const QString configPath = QDir(configDirectory).filePath(QStringLiteral("thconfig"));
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(configPath,
                                                      false,
                                                      manualDirectory,
                                                      tempDir.path(),
                                                      QString());

    if (!expect(!result.shouldUpdateWorkingDirectoryText,
                "Manual working-directory edits should not be overwritten by config auto-resolution.")) {
        return 1;
    }
    if (!expect(result.shouldUpdateAutoResolvedWorkingDirectory
                    && result.updatedAutoResolvedWorkingDirectory == configDirectory,
                "Auto-resolved marker should still track the latest config directory.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runEmptyResolvedPathClearsAutoWorkingDirectoryTest() != 0) {
        return 1;
    }
    if (runExplicitConfigDoesNotAutoUpdateWorkingDirectoryTest() != 0) {
        return 1;
    }
    if (runBlankWorkingDirectoryAutoUpdatesToConfigDirectoryTest() != 0) {
        return 1;
    }
    if (runProjectRootWorkingDirectoryAutoUpdatesToConfigDirectoryTest() != 0) {
        return 1;
    }
    if (runManualWorkingDirectoryIsPreservedTest() != 0) {
        return 1;
    }

    return 0;
}
