#include "../src/app/MainWindowProjectWorkspaceService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowProjectWorkspaceServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsOpenProjectWorkspaceState();
    void buildsCloseProjectWorkspaceState();
};

void MainWindowProjectWorkspaceServiceTest::buildsOpenProjectWorkspaceState()
{
    const auto noTabs = MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
        QStringLiteral("/tmp/project"), true, false);
    QCOMPARE(noTabs.projectRootPath, QStringLiteral("/tmp/project"));
    QCOMPARE(noTabs.sessionLastProjectPath, QStringLiteral("/tmp/project"));
    QVERIFY(noTabs.shouldEnsureWelcomeTab);

    const auto hasWelcome = MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
        QStringLiteral("/tmp/project"), false, true);
    QVERIFY(hasWelcome.shouldEnsureWelcomeTab);

    const auto hasRegularTabs = MainWindowProjectWorkspaceService::buildOpenProjectWorkspaceState(
        QStringLiteral("/tmp/project"), false, false);
    QVERIFY(!hasRegularTabs.shouldEnsureWelcomeTab);
}

void MainWindowProjectWorkspaceServiceTest::buildsCloseProjectWorkspaceState()
{
    const auto close = MainWindowProjectWorkspaceService::buildCloseProjectWorkspaceState(
        QStringLiteral("/tmp/project"));
    QVERIFY(close.projectRootPath.isEmpty());
    QVERIFY(close.sessionLastProjectPath.isEmpty());
    QCOMPARE(close.closedProjectPath, QStringLiteral("/tmp/project"));
}
}

int runMainWindowProjectWorkspaceServiceTest(int argc, char **argv)
{
    MainWindowProjectWorkspaceServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowProjectWorkspaceServiceTest.moc"
