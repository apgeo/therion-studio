#include "TherionRunnerStartSuccessPresenter.h"

#include <QCoreApplication>

namespace
{
QString translate(const char *sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}
}

namespace TherionStudio
{
TherionRunnerStartSuccessPresenter::Presentation
TherionRunnerStartSuccessPresenter::present(const TherionRunnerService::StartResult &startResult,
                                            const QString &executableInput,
                                            const QString &argumentsText,
                                            const QString &workingDirectory)
{
    Presentation result;

    result.consoleMessage =
        translate("Running %1 %2 in %3").arg(startResult.resolvedExecutablePath, argumentsText, workingDirectory);
    result.statusLabelMessage = translate("Starting Therion...");
    return result;
}
}
