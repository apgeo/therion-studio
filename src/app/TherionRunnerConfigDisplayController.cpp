#include "TherionRunnerConfigDisplayController.h"

#include <QFileInfo>

namespace TherionStudio
{
TherionRunnerConfigDisplayController::RefreshComputation
TherionRunnerConfigDisplayController::compute(const QString &resolvedPath)
{
    RefreshComputation result;

    if (resolvedPath.isEmpty()) {
        return result;
    }

    const QFileInfo configInfo(resolvedPath);
    result.hasResolvedConfigPath = true;
    result.configName = configInfo.fileName().isEmpty() ? QStringLiteral("thconfig") : configInfo.fileName();
    result.configPath = resolvedPath;
    result.configDirectory = configInfo.absolutePath();
    return result;
}
}
