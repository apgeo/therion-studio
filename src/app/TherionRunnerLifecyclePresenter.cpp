#include "TherionRunnerLifecyclePresenter.h"

#include <QCoreApplication>

namespace TherionStudio
{
TherionRunnerLifecyclePresenter::StopPresentation
TherionRunnerLifecyclePresenter::presentStopRequest(bool isRunning)
{
    StopPresentation result;

    if (!isRunning) {
        result.shouldUpdateStatusLabel = true;
        result.statusLabelMessage = QCoreApplication::translate("MainWindow", "Therion is not running.");
        return result;
    }

    result.shouldStopProcess = true;
    result.shouldUpdateStatusLabel = true;
    result.statusLabelMessage = QCoreApplication::translate("MainWindow", "Stopping Therion...");
    return result;
}

TherionRunnerLifecyclePresenter::EventPresentation
TherionRunnerLifecyclePresenter::presentFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    EventPresentation result;
    result.statusText = exitStatus == QProcess::NormalExit
        ? QCoreApplication::translate("MainWindow", "Therion finished with exit code %1.").arg(exitCode)
        : QCoreApplication::translate("MainWindow", "Therion crashed while running.");
    return result;
}

TherionRunnerLifecyclePresenter::EventPresentation
TherionRunnerLifecyclePresenter::presentError(const QString &errorText)
{
    EventPresentation result;
    result.statusText = QCoreApplication::translate("MainWindow", "Therion runner error: %1").arg(errorText);
    return result;
}
}
