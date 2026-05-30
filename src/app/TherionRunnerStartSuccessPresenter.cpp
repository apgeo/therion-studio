#include "TherionRunnerStartSuccessPresenter.h"

#include <QCoreApplication>

namespace TherionStudio
{
TherionRunnerStartSuccessPresenter::Presentation
TherionRunnerStartSuccessPresenter::present(const TherionRunnerService::StartResult &startResult,
                                            const QString &argumentsText,
                                            const QString &workingDirectory)
{
    Presentation result;

    result.consoleMessage =
        QCoreApplication::translate("MainWindow", "Running %1 %2 in %3").arg(startResult.resolvedExecutablePath, argumentsText, workingDirectory);
    result.statusLabelMessage = QCoreApplication::translate("MainWindow", "Starting Therion...");
    return result;
}
}
