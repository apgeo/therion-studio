#include "MainWindowProjectUiFlowService.h"

#include <QCoreApplication>

namespace TherionStudio
{
MainWindowProjectUiFlowService::DecisionPresentation MainWindowProjectUiFlowService::presentOpenProjectDecision(const MainWindowProjectLifecycleService::OpenProjectDecision &decision)
{
    DecisionPresentation presentation;

    switch (decision.status) {
    case MainWindowProjectLifecycleService::OpenProjectStatus::Cancelled:
        presentation.showStatusBarMessage = true;
        presentation.statusBarMessage = QCoreApplication::translate("MainWindow", "Open project cancelled");
        presentation.statusBarTimeoutMs = 2000;
        return presentation;
    case MainWindowProjectLifecycleService::OpenProjectStatus::MissingDirectory:
        presentation.showWarningDialog = true;
        presentation.warningDialogTitle = QCoreApplication::translate("MainWindow", "Open Project");
        presentation.warningDialogMessage = QCoreApplication::translate("MainWindow", "The selected folder does not exist.");
        return presentation;
    case MainWindowProjectLifecycleService::OpenProjectStatus::OpenProject:
        presentation.shouldContinueWorkflow = true;
        return presentation;
    }

    return presentation;
}

MainWindowProjectUiFlowService::SuccessPresentation MainWindowProjectUiFlowService::presentOpenProjectSuccess(const QString &projectRootPath)
{
    SuccessPresentation presentation;
    presentation.statusBarMessage = QCoreApplication::translate("MainWindow", "Project root set to %1").arg(projectRootPath);
    presentation.statusBarTimeoutMs = 3000;
    presentation.consoleMessage = QCoreApplication::translate("MainWindow", "Project root set to %1").arg(projectRootPath);
    return presentation;
}

MainWindowProjectUiFlowService::DecisionPresentation MainWindowProjectUiFlowService::presentCloseProjectDecision(const MainWindowProjectLifecycleService::CloseProjectDecision &decision)
{
    DecisionPresentation presentation;

    switch (decision.status) {
    case MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen:
        presentation.showStatusBarMessage = true;
        presentation.statusBarMessage = QCoreApplication::translate("MainWindow", "No project is open");
        presentation.statusBarTimeoutMs = 2000;
        return presentation;
    case MainWindowProjectLifecycleService::CloseProjectStatus::CancelledByDirtyDocuments:
        return presentation;
    case MainWindowProjectLifecycleService::CloseProjectStatus::CloseProject:
        presentation.shouldContinueWorkflow = true;
        return presentation;
    }

    return presentation;
}

MainWindowProjectUiFlowService::SuccessPresentation MainWindowProjectUiFlowService::presentCloseProjectSuccess(const QString &closedProjectPath)
{
    SuccessPresentation presentation;
    presentation.statusBarMessage = QCoreApplication::translate("MainWindow", "Project closed");
    presentation.statusBarTimeoutMs = 3000;
    presentation.consoleMessage = QCoreApplication::translate("MainWindow", "Closed project %1").arg(closedProjectPath);
    return presentation;
}
}
