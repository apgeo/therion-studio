#include "MainWindowProjectUiFlowService.h"

#include <QCoreApplication>

namespace TherionStudio
{
namespace
{
QString trMainWindow(const char *sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}
}

MainWindowProjectUiFlowService::DecisionPresentation MainWindowProjectUiFlowService::presentOpenProjectDecision(const MainWindowProjectLifecycleService::OpenProjectDecision &decision)
{
    DecisionPresentation presentation;

    switch (decision.status) {
    case MainWindowProjectLifecycleService::OpenProjectStatus::Cancelled:
        presentation.showStatusBarMessage = true;
        presentation.statusBarMessage = trMainWindow("Open project cancelled");
        presentation.statusBarTimeoutMs = 2000;
        return presentation;
    case MainWindowProjectLifecycleService::OpenProjectStatus::MissingDirectory:
        presentation.showWarningDialog = true;
        presentation.warningDialogTitle = trMainWindow("Open Project");
        presentation.warningDialogMessage = trMainWindow("The selected folder does not exist.");
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
    presentation.statusBarMessage = trMainWindow("Project root set to %1").arg(projectRootPath);
    presentation.statusBarTimeoutMs = 3000;
    presentation.consoleMessage = trMainWindow("Project root set to %1").arg(projectRootPath);
    return presentation;
}

MainWindowProjectUiFlowService::DecisionPresentation MainWindowProjectUiFlowService::presentCloseProjectDecision(const MainWindowProjectLifecycleService::CloseProjectDecision &decision)
{
    DecisionPresentation presentation;

    switch (decision.status) {
    case MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen:
        presentation.showStatusBarMessage = true;
        presentation.statusBarMessage = trMainWindow("No project is open");
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
    presentation.statusBarMessage = trMainWindow("Project closed");
    presentation.statusBarTimeoutMs = 3000;
    presentation.consoleMessage = trMainWindow("Closed project %1").arg(closedProjectPath);
    return presentation;
}
}
