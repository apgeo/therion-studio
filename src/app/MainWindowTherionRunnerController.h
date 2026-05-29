#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class MainWindowTherionRunnerController final
{
public:
    struct RuntimeInput
    {
        QString projectRootPath;
        QString executableText;
        QString workingDirectoryOverrideText;
        QString argumentsText;
        QString runTargetMode;
        QString targetConfigPathText;
        QString currentDocumentPath;
    };

    struct RuntimeState
    {
        bool currentConfigAvailable = false;
        QString effectiveRunTargetMode;
        bool projectConfigActive = true;
        bool hasWorkingDirectoryOverride = false;
        bool hasExplicitConfigArgument = false;
        QString currentDocumentConfigPath;
        QString configResolutionDirectory;
        QString resolvedTargetConfigPath;
        QString resolvedConfigPath;
        QString resolvedWorkingDirectory;
        QString executableInput;
        QStringList baseArguments;
        QStringList runArguments;
        bool canRun = false;
    };

    static RuntimeState computeRuntimeState(const RuntimeInput &input);
    static QString currentDocumentConfigPath(const QString &currentDocumentPath);
};
}
