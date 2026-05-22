#include "../src/app/TherionRunnerLifecyclePresenter.h"
#include "../src/app/TherionRunnerStartResultPresenter.h"
#include "../src/app/TherionRunnerStartSuccessPresenter.h"

#include <QCoreApplication>

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

int runStartResultPresentationTest()
{
    const TherionRunnerStartResultPresenter::Presentation started =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::Started,
                                                   QStringLiteral("therion"));
    if (!expect(!started.isHandled, "Started result should be left for success handling.")) {
        return 1;
    }

    const TherionRunnerStartResultPresenter::Presentation alreadyRunning =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::AlreadyRunning,
                                                   QStringLiteral("therion"));
    if (!expect(alreadyRunning.isHandled, "AlreadyRunning should be handled by the start-result presenter.")) {
        return 1;
    }
    if (!expect(alreadyRunning.appendConsoleMessage
                    && alreadyRunning.showStatusBarMessage
                    && alreadyRunning.updateStatusLabel,
                "AlreadyRunning should update console, status bar, and status label.")) {
        return 1;
    }
    if (!expect(alreadyRunning.statusBarTimeoutMs == 3000,
                "AlreadyRunning status-bar timeout should remain 3000 ms.")) {
        return 1;
    }

    const TherionRunnerStartResultPresenter::Presentation missingWorkingDirectory =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::MissingWorkingDirectory,
                                                   QStringLiteral("therion"));
    if (!expect(missingWorkingDirectory.showWarningDialog,
                "MissingWorkingDirectory should request a warning dialog.")) {
        return 1;
    }
    if (!expect(missingWorkingDirectory.warningDialogMessage
                    == QStringLiteral("Open a project or set a working directory before running Therion."),
                "MissingWorkingDirectory warning message changed unexpectedly.")) {
        return 1;
    }

    const TherionRunnerStartResultPresenter::Presentation missingExecutable =
        TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode::ExecutableNotFound,
                                                   QStringLiteral("/missing/therion"));
    if (!expect(missingExecutable.showWarningDialog
                    && missingExecutable.appendConsoleMessage
                    && missingExecutable.updateStatusLabel,
                "ExecutableNotFound should warn and mirror the message to console/status label.")) {
        return 1;
    }
    if (!expect(missingExecutable.warningDialogMessage.contains(QStringLiteral("/missing/therion")),
                "ExecutableNotFound message should include the executable input.")) {
        return 1;
    }

    return 0;
}

int runStartSuccessPresentationTest()
{
    TherionRunnerService::StartResult startResult;
    startResult.code = TherionRunnerService::StartCode::Started;
    startResult.resolvedExecutablePath = QStringLiteral("/opt/therion/bin/therion");
    startResult.usedPlatformFallback = true;

    const TherionRunnerStartSuccessPresenter::Presentation fallbackPresentation =
        TherionRunnerStartSuccessPresenter::present(startResult,
                                                    QStringLiteral("therion"),
                                                    QStringLiteral("-q survey.th"),
                                                    QStringLiteral("/project"));
    if (!expect(fallbackPresentation.shouldUpdateExecutableText,
                "Platform fallback should update executable text when the user input was the default token.")) {
        return 1;
    }
    if (!expect(fallbackPresentation.updatedExecutableText == startResult.resolvedExecutablePath,
                "Platform fallback should write the resolved executable path back to the UI field.")) {
        return 1;
    }
    if (!expect(fallbackPresentation.consoleMessage
                    == QStringLiteral("Running /opt/therion/bin/therion -q survey.th in /project"),
                "Start-success console message changed unexpectedly.")) {
        return 1;
    }
    if (!expect(fallbackPresentation.statusLabelMessage == QStringLiteral("Starting Therion..."),
                "Start-success status label message changed unexpectedly.")) {
        return 1;
    }

    const TherionRunnerStartSuccessPresenter::Presentation explicitPresentation =
        TherionRunnerStartSuccessPresenter::present(startResult,
                                                    QStringLiteral("/custom/therion"),
                                                    QString(),
                                                    QStringLiteral("/project"));
    if (!expect(!explicitPresentation.shouldUpdateExecutableText,
                "Explicit executable input should not be overwritten by platform fallback.")) {
        return 1;
    }

    return 0;
}

int runLifecyclePresentationTest()
{
    const TherionRunnerLifecyclePresenter::StopPresentation notRunning =
        TherionRunnerLifecyclePresenter::presentStopRequest(false);
    if (!expect(!notRunning.shouldStopProcess, "Stop request should not stop when Therion is not running.")) {
        return 1;
    }
    if (!expect(notRunning.consoleMessage == QStringLiteral("Therion is not running."),
                "Not-running stop message changed unexpectedly.")) {
        return 1;
    }

    const TherionRunnerLifecyclePresenter::StopPresentation running =
        TherionRunnerLifecyclePresenter::presentStopRequest(true);
    if (!expect(running.shouldStopProcess
                    && running.shouldUpdateStatusLabel
                    && running.statusLabelMessage == QStringLiteral("Stopping Therion..."),
                "Running stop request should stop the process and update the status label.")) {
        return 1;
    }

    const TherionRunnerLifecyclePresenter::EventPresentation normalFinish =
        TherionRunnerLifecyclePresenter::presentFinished(7, QProcess::NormalExit);
    if (!expect(normalFinish.statusText == QStringLiteral("Therion finished with exit code 7."),
                "Normal finish status text changed unexpectedly.")) {
        return 1;
    }

    const TherionRunnerLifecyclePresenter::EventPresentation crashFinish =
        TherionRunnerLifecyclePresenter::presentFinished(1, QProcess::CrashExit);
    if (!expect(crashFinish.statusText == QStringLiteral("Therion crashed while running."),
                "Crash finish status text changed unexpectedly.")) {
        return 1;
    }

    const TherionRunnerLifecyclePresenter::EventPresentation error =
        TherionRunnerLifecyclePresenter::presentError(QStringLiteral("permission denied"));
    if (!expect(error.statusText == QStringLiteral("Therion runner error: permission denied"),
                "Runner error status text changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runStartResultPresentationTest() != 0) {
        return 1;
    }
    if (runStartSuccessPresentationTest() != 0) {
        return 1;
    }
    if (runLifecyclePresentationTest() != 0) {
        return 1;
    }

    return 0;
}
