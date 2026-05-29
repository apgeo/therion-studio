#include "../src/app/MainWindowProjectLifecycleService.h"
#include "../src/app/MainWindowProjectUiFlowService.h"

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

int runOpenProjectPresentationTest()
{
    MainWindowProjectLifecycleService::OpenProjectDecision cancelled;
    cancelled.status = MainWindowProjectLifecycleService::OpenProjectStatus::Cancelled;
    const auto cancelledPresentation = MainWindowProjectUiFlowService::presentOpenProjectDecision(cancelled);
    if (!expect(!cancelledPresentation.shouldContinueWorkflow,
                "Cancelled open-project should not continue workflow.")) {
        return 1;
    }
    if (!expect(cancelledPresentation.showStatusBarMessage
                    && cancelledPresentation.statusBarMessage == QStringLiteral("Open project cancelled")
                    && cancelledPresentation.statusBarTimeoutMs == 2000,
                "Cancelled open-project should show status bar message with expected timeout.")) {
        return 1;
    }

    MainWindowProjectLifecycleService::OpenProjectDecision missing;
    missing.status = MainWindowProjectLifecycleService::OpenProjectStatus::MissingDirectory;
    const auto missingPresentation = MainWindowProjectUiFlowService::presentOpenProjectDecision(missing);
    if (!expect(!missingPresentation.shouldContinueWorkflow
                    && missingPresentation.showWarningDialog
                    && missingPresentation.warningDialogTitle == QStringLiteral("Open Project")
                    && missingPresentation.warningDialogMessage == QStringLiteral("The selected folder does not exist."),
                "Missing-directory open-project should request expected warning dialog.")) {
        return 1;
    }

    MainWindowProjectLifecycleService::OpenProjectDecision open;
    open.status = MainWindowProjectLifecycleService::OpenProjectStatus::OpenProject;
    const auto openPresentation = MainWindowProjectUiFlowService::presentOpenProjectDecision(open);
    if (!expect(openPresentation.shouldContinueWorkflow,
                "Open-project decision should continue workflow for valid project.")) {
        return 1;
    }

    const auto openSuccess = MainWindowProjectUiFlowService::presentOpenProjectSuccess(QStringLiteral("/tmp/project"));
    if (!expect(openSuccess.statusBarMessage == QStringLiteral("Project root set to /tmp/project")
                    && openSuccess.statusBarTimeoutMs == 3000
                    && openSuccess.consoleMessage == QStringLiteral("Project root set to /tmp/project"),
                "Open-project success presentation changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectPresentationTest()
{
    MainWindowProjectLifecycleService::CloseProjectDecision noProject;
    noProject.status = MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen;
    const auto noProjectPresentation = MainWindowProjectUiFlowService::presentCloseProjectDecision(noProject);
    if (!expect(!noProjectPresentation.shouldContinueWorkflow
                    && noProjectPresentation.showStatusBarMessage
                    && noProjectPresentation.statusBarMessage == QStringLiteral("No project is open")
                    && noProjectPresentation.statusBarTimeoutMs == 2000,
                "No-project close should show expected status bar message.")) {
        return 1;
    }

    MainWindowProjectLifecycleService::CloseProjectDecision cancelled;
    cancelled.status = MainWindowProjectLifecycleService::CloseProjectStatus::CancelledByDirtyDocuments;
    const auto cancelledPresentation = MainWindowProjectUiFlowService::presentCloseProjectDecision(cancelled);
    if (!expect(!cancelledPresentation.shouldContinueWorkflow
                    && !cancelledPresentation.showStatusBarMessage
                    && !cancelledPresentation.showWarningDialog,
                "Dirty-document cancelled close should not emit additional UI messages.")) {
        return 1;
    }

    MainWindowProjectLifecycleService::CloseProjectDecision close;
    close.status = MainWindowProjectLifecycleService::CloseProjectStatus::CloseProject;
    const auto closePresentation = MainWindowProjectUiFlowService::presentCloseProjectDecision(close);
    if (!expect(closePresentation.shouldContinueWorkflow,
                "Close-project decision should continue workflow when allowed.")) {
        return 1;
    }

    const auto closeSuccess = MainWindowProjectUiFlowService::presentCloseProjectSuccess(QStringLiteral("/tmp/project"));
    if (!expect(closeSuccess.statusBarMessage == QStringLiteral("Project closed")
                    && closeSuccess.statusBarTimeoutMs == 3000
                    && closeSuccess.consoleMessage == QStringLiteral("Closed project /tmp/project"),
                "Close-project success presentation changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runOpenProjectPresentationTest() != 0) {
        return 1;
    }
    if (runCloseProjectPresentationTest() != 0) {
        return 1;
    }

    return 0;
}
