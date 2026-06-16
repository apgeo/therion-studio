#include "../src/app/MainWindowSessionStateService.h"
#include "FakeSessionStore.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowSessionStateServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void persistsMainWindowState();
    void persistsOpenDocuments();
};

void MainWindowSessionStateServiceTest::persistsMainWindowState()
{
    FakeSessionStore sessionStore;

    MainWindowSessionStateService::MainWindowStateSnapshot snapshot;
    snapshot.windowGeometry = QByteArrayLiteral("geometry");
    snapshot.windowState = QByteArrayLiteral("state");
    snapshot.projectRootPath = QStringLiteral("/tmp/project");
    snapshot.therionExecutablePath = QStringLiteral("/opt/therion");
    snapshot.therionWorkingDirectory = QStringLiteral("/tmp/project/work");
    snapshot.therionRunTargetMode = QStringLiteral("current");
    snapshot.therionTargetConfigPath = QStringLiteral("/tmp/project/thconfig");
    snapshot.structureNameOverridesJson = QStringLiteral("{\"a\":\"b\"}");

    MainWindowSessionStateService::persistMainWindowState(sessionStore, snapshot);

    QCOMPARE(sessionStore.mainWindowGeometry(), snapshot.windowGeometry);
    QCOMPARE(sessionStore.mainWindowState(), snapshot.windowState);
    QCOMPARE(sessionStore.lastProjectPath(), snapshot.projectRootPath);
    QCOMPARE(sessionStore.therionExecutablePath(), snapshot.therionExecutablePath);
    QCOMPARE(sessionStore.therionWorkingDirectory(), snapshot.therionWorkingDirectory);
    QCOMPARE(sessionStore.therionRunTargetMode(), snapshot.therionRunTargetMode);
    QCOMPARE(sessionStore.therionTargetConfigPath(), snapshot.therionTargetConfigPath);
    QCOMPARE(sessionStore.structureNameOverrides(), snapshot.structureNameOverridesJson);
}

void MainWindowSessionStateServiceTest::persistsOpenDocuments()
{
    FakeSessionStore sessionStore;

    MainWindowSessionStateService::OpenDocumentsSnapshot snapshot;
    snapshot.openDocumentPaths = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th2")};
    snapshot.activeDocumentPath = QStringLiteral("/tmp/b.th2");

    MainWindowSessionStateService::persistOpenDocuments(sessionStore, snapshot);

    QCOMPARE(sessionStore.openDocumentPaths(), snapshot.openDocumentPaths);
    QCOMPARE(sessionStore.activeDocumentPath(), snapshot.activeDocumentPath);
}
}

int runMainWindowSessionStateServiceTest(int argc, char **argv)
{
    MainWindowSessionStateServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowSessionStateServiceTest.moc"
