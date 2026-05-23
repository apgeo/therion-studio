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
    };

    static RefreshComputation compute(const QString &resolvedPath);
};
}
