#include "TherionRunnerLifecyclePresenter.h"

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
TherionRunnerLifecyclePresenter::StopPresentation
TherionRunnerLifecyclePresenter::presentStopRequest(bool isRunning)
{
    StopPresentation result;

    if (!isRunning) {
        result.consoleMessage = translate("Therion is not running.");
        return result;
    }

    result.shouldStopProcess = true;
    result.consoleMessage = translate("Stopping Therion runner...");
    result.shouldUpdateStatusLabel = true;
    result.statusLabelMessage = translate("Stopping Therion...");
    return result;
}

TherionRunnerLifecyclePresenter::EventPresentation
TherionRunnerLifecyclePresenter::presentFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    EventPresentation result;
    result.statusText = exitStatus == QProcess::NormalExit
        ? translate("Therion finished with exit code %1.").arg(exitCode)
        : translate("Therion crashed while running.");
    return result;
}

TherionRunnerLifecyclePresenter::EventPresentation
TherionRunnerLifecyclePresenter::presentError(const QString &errorText)
{
    EventPresentation result;
    result.statusText = translate("Therion runner error: %1").arg(errorText);
    return result;
}
}
