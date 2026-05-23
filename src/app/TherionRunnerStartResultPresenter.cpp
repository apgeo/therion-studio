#include "TherionRunnerStartResultPresenter.h"

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
TherionRunnerStartResultPresenter::Presentation
TherionRunnerStartResultPresenter::present(TherionRunnerService::StartCode code,
                                           const QString &executableInput)
{
    Presentation result;

    switch (code) {
    case TherionRunnerService::StartCode::AlreadyRunning: {
        const QString message =
            translate("Therion is already running. Parallel runs are rejected until the current run finishes.");
        result.isHandled = true;
        result.showStatusBarMessage = true;
        result.statusBarMessage = message;
        result.statusBarTimeoutMs = 3000;
        result.updateStatusLabel = true;
        result.statusLabelMessage = message;
        return result;
    }
    case TherionRunnerService::StartCode::MissingWorkingDirectory:
        result.isHandled = true;
        result.showWarningDialog = true;
        result.warningDialogTitle = translate("Run Therion");
        result.warningDialogMessage =
            translate("Open a project or set a working directory before running Therion.");
        return result;
    case TherionRunnerService::StartCode::WorkingDirectoryNotFound:
        result.isHandled = true;
        result.showWarningDialog = true;
        result.warningDialogTitle = translate("Run Therion");
        result.warningDialogMessage =
            translate("The selected working directory does not exist.");
        return result;
    case TherionRunnerService::StartCode::ExecutableNotFound: {
        const QString message =
            translate("Therion executable \"%1\" was not found or is not executable. Set Executable to a full path or install Therion in the application PATH.")
                .arg(executableInput);
        result.isHandled = true;
        result.showWarningDialog = true;
        result.warningDialogTitle = translate("Run Therion");
        result.warningDialogMessage = message;
        result.updateStatusLabel = true;
        result.statusLabelMessage = message;
        return result;
    }
    case TherionRunnerService::StartCode::Started:
        return result;
    }

    return result;
}
}
