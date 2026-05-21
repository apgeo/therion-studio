#pragma once

#include <QString>

namespace TherionStudio
{
class TherionRunnerConfigDisplayController final
{
public:
    struct RefreshComputation
    {
        bool hasResolvedConfigPath = false;
        QString configName;
        QString configPath;
        QString configDirectory;
        bool shouldClearAutoResolvedWorkingDirectory = false;
        bool shouldUpdateAutoResolvedWorkingDirectory = false;
        QString updatedAutoResolvedWorkingDirectory;
        bool shouldUpdateWorkingDirectoryText = false;
        QString updatedWorkingDirectoryText;
    };

    static RefreshComputation compute(const QString &resolvedPath,
                                      bool explicitConfigArgument,
                                      const QString &currentWorkingDirectoryText,
                                      const QString &projectRootPath,
                                      const QString &autoResolvedWorkingDirectory);
};
}
