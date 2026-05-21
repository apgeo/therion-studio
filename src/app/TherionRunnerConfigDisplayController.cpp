#include "TherionRunnerConfigDisplayController.h"

#include <QDir>
#include <QFileInfo>

namespace TherionStudio
{
TherionRunnerConfigDisplayController::RefreshComputation
TherionRunnerConfigDisplayController::compute(const QString &resolvedPath,
                                              bool explicitConfigArgument,
                                              const QString &currentWorkingDirectoryText,
                                              const QString &projectRootPath,
                                              const QString &autoResolvedWorkingDirectory)
{
    RefreshComputation result;

    if (resolvedPath.isEmpty()) {
        result.shouldClearAutoResolvedWorkingDirectory = true;
        return result;
    }

    const QFileInfo configInfo(resolvedPath);
    result.hasResolvedConfigPath = true;
    result.configName = configInfo.fileName().isEmpty() ? QStringLiteral("thconfig") : configInfo.fileName();
    result.configPath = resolvedPath;
    result.configDirectory = configInfo.absolutePath();

    if (explicitConfigArgument) {
        return result;
    }

    const QString normalizedCurrentWorkingDirectory = QDir(currentWorkingDirectoryText.trimmed()).absolutePath();
    const QString normalizedProjectRoot = projectRootPath.isEmpty() ? QString() : QDir(projectRootPath).absolutePath();
    const QString normalizedAutoResolvedWorkingDirectory = autoResolvedWorkingDirectory.isEmpty()
        ? QString()
        : QDir(autoResolvedWorkingDirectory).absolutePath();

    const bool shouldAutoUpdateWorkingDirectory = currentWorkingDirectoryText.trimmed().isEmpty()
        || (!normalizedProjectRoot.isEmpty() && normalizedCurrentWorkingDirectory == normalizedProjectRoot)
        || (!normalizedAutoResolvedWorkingDirectory.isEmpty()
            && normalizedCurrentWorkingDirectory == normalizedAutoResolvedWorkingDirectory);

    if (shouldAutoUpdateWorkingDirectory && normalizedCurrentWorkingDirectory != result.configDirectory) {
        result.shouldUpdateWorkingDirectoryText = true;
        result.updatedWorkingDirectoryText = result.configDirectory;
    }

    result.shouldUpdateAutoResolvedWorkingDirectory = true;
    result.updatedAutoResolvedWorkingDirectory = result.configDirectory;
    return result;
}
}
