#include "../src/app/MainWindowProjectLifecycleService.h"
#include "../src/app/MainWindowProjectUiFlowService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowProjectUiFlowServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void presentsOpenProjectDecisions();
    void presentsCloseProjectDecisions();
};

void MainWindowProjectUiFlowServiceTest::presentsOpenProjectDecisions()
{
    MainWindowProjectLifecycleService::OpenProjectDecision cancelled;
    cancelled.status = MainWindowProjectLifecycleService::OpenProjectStatus::Cancelled;
    const auto cancelledPresentation = MainWindowProjectUiFlowService::presentOpenProjectDecision(cancelled);
    QVERIFY(!cancelledPresentation.shouldContinueWorkflow);
    QVERIFY(cancelledPresentation.showStatusBarMessage);
    QCOMPARE(cancelledPresentation.statusBarMessage, QStringLiteral("Open project cancelled"));
    QCOMPARE(cancelledPresentation.statusBarTimeoutMs, 2000);

    MainWindowProjectLifecycleService::OpenProjectDecision missing;
    missing.status = MainWindowProjectLifecycleService::OpenProjectStatus::MissingDirectory;
    const auto missingPresentation = MainWindowProjectUiFlowService::presentOpenProjectDecision(missing);
    QVERIFY(!missingPresentation.shouldContinueWorkflow);
    QVERIFY(missingPresentation.showWarningDialog);
    QCOMPARE(missingPresentation.warningDialogTitle, QStringLiteral("Open Project"));
    QCOMPARE(missingPresentation.warningDialogMessage, QStringLiteral("The selected folder does not exist."));

    MainWindowProjectLifecycleService::OpenProjectDecision open;
    open.status = MainWindowProjectLifecycleService::OpenProjectStatus::OpenProject;
    const auto openPresentation = MainWindowProjectUiFlowService::presentOpenProjectDecision(open);
    QVERIFY(openPresentation.shouldContinueWorkflow);

    const auto openSuccess = MainWindowProjectUiFlowService::presentOpenProjectSuccess(QStringLiteral("/tmp/project"));
    QCOMPARE(openSuccess.statusBarMessage, QStringLiteral("Project root set to /tmp/project"));
    QCOMPARE(openSuccess.statusBarTimeoutMs, 3000);
    QCOMPARE(openSuccess.consoleMessage, QStringLiteral("Project root set to /tmp/project"));
}

void MainWindowProjectUiFlowServiceTest::presentsCloseProjectDecisions()
{
    MainWindowProjectLifecycleService::CloseProjectDecision noProject;
    noProject.status = MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen;
    const auto noProjectPresentation = MainWindowProjectUiFlowService::presentCloseProjectDecision(noProject);
    QVERIFY(!noProjectPresentation.shouldContinueWorkflow);
    QVERIFY(noProjectPresentation.showStatusBarMessage);
    QCOMPARE(noProjectPresentation.statusBarMessage, QStringLiteral("No project is open"));
    QCOMPARE(noProjectPresentation.statusBarTimeoutMs, 2000);

    MainWindowProjectLifecycleService::CloseProjectDecision cancelled;
    cancelled.status = MainWindowProjectLifecycleService::CloseProjectStatus::CancelledByDirtyDocuments;
    const auto cancelledPresentation = MainWindowProjectUiFlowService::presentCloseProjectDecision(cancelled);
    QVERIFY(!cancelledPresentation.shouldContinueWorkflow);
    QVERIFY(!cancelledPresentation.showStatusBarMessage);
    QVERIFY(!cancelledPresentation.showWarningDialog);

    MainWindowProjectLifecycleService::CloseProjectDecision close;
    close.status = MainWindowProjectLifecycleService::CloseProjectStatus::CloseProject;
    const auto closePresentation = MainWindowProjectUiFlowService::presentCloseProjectDecision(close);
    QVERIFY(closePresentation.shouldContinueWorkflow);

    const auto closeSuccess = MainWindowProjectUiFlowService::presentCloseProjectSuccess(QStringLiteral("/tmp/project"));
    QCOMPARE(closeSuccess.statusBarMessage, QStringLiteral("Project closed"));
    QCOMPARE(closeSuccess.statusBarTimeoutMs, 3000);
    QCOMPARE(closeSuccess.consoleMessage, QStringLiteral("Closed project /tmp/project"));
}
}

int runMainWindowProjectUiFlowServiceTest(int argc, char **argv)
{
    MainWindowProjectUiFlowServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowProjectUiFlowServiceTest.moc"
