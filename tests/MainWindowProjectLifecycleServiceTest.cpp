#include "../src/app/MainWindowProjectLifecycleService.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowProjectLifecycleServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void decidesOpenProject();
    void decidesCloseProject();
    void decidesCloseProjectWithCallback();
};

void MainWindowProjectLifecycleServiceTest::decidesOpenProject()
{
    const auto cancelled = MainWindowProjectLifecycleService::decideOpenProject(QString());
    QCOMPARE(cancelled.status, MainWindowProjectLifecycleService::OpenProjectStatus::Cancelled);

    const auto missing =
        MainWindowProjectLifecycleService::decideOpenProject(QStringLiteral("/definitely/missing/project/path"));
    QCOMPARE(missing.status, MainWindowProjectLifecycleService::OpenProjectStatus::MissingDirectory);

    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    const auto open = MainWindowProjectLifecycleService::decideOpenProject(tempDir.path());
    QCOMPARE(open.status, MainWindowProjectLifecycleService::OpenProjectStatus::OpenProject);
    QCOMPARE(open.projectPath, QDir(tempDir.path()).absolutePath());
}

void MainWindowProjectLifecycleServiceTest::decidesCloseProject()
{
    const auto noProject = MainWindowProjectLifecycleService::decideCloseProject(QString(), true);
    QCOMPARE(noProject.status, MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen);

    const auto cancelled = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"), false);
    QCOMPARE(cancelled.status, MainWindowProjectLifecycleService::CloseProjectStatus::CancelledByDirtyDocuments);

    const auto close = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"), true);
    QCOMPARE(close.status, MainWindowProjectLifecycleService::CloseProjectStatus::CloseProject);
    QCOMPARE(close.closedProjectPath, QStringLiteral("/tmp/project"));
}

void MainWindowProjectLifecycleServiceTest::decidesCloseProjectWithCallback()
{
    int callbackCallCount = 0;
    const auto noProject = MainWindowProjectLifecycleService::decideCloseProject(QString(), [&callbackCallCount]() {
        ++callbackCallCount;
        return true;
    });
    QCOMPARE(noProject.status, MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen);
    QCOMPARE(callbackCallCount, 0);

    callbackCallCount = 0;
    const auto cancelled = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"),
                                                                                  [&callbackCallCount]() {
                                                                                      ++callbackCallCount;
                                                                                      return false;
                                                                                  });
    QCOMPARE(cancelled.status, MainWindowProjectLifecycleService::CloseProjectStatus::CancelledByDirtyDocuments);
    QCOMPARE(callbackCallCount, 1);

    callbackCallCount = 0;
    const auto close = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"),
                                                                              [&callbackCallCount]() {
                                                                                  ++callbackCallCount;
                                                                                  return true;
                                                                              });
    QCOMPARE(close.status, MainWindowProjectLifecycleService::CloseProjectStatus::CloseProject);
    QCOMPARE(callbackCallCount, 1);
}
}

int runMainWindowProjectLifecycleServiceTest(int argc, char **argv)
{
    MainWindowProjectLifecycleServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowProjectLifecycleServiceTest.moc"
