#include "../src/app/MainWindowSessionWindowRestoreService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowSessionWindowRestoreServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsPlan();
    void decidesResizeFallback();
};

void MainWindowSessionWindowRestoreServiceTest::buildsPlan()
{
    const auto emptyPlan = MainWindowSessionWindowRestoreService::buildPlan({}, {});
    QVERIFY(!emptyPlan.shouldAttemptRestoreGeometry);
    QVERIFY(!emptyPlan.shouldAttemptRestoreState);
    QVERIFY(emptyPlan.shouldEnsureUsableWindowSize);

    const auto fullPlan = MainWindowSessionWindowRestoreService::buildPlan(
        QByteArrayLiteral("geom"), QByteArrayLiteral("state"));
    QVERIFY(fullPlan.shouldAttemptRestoreGeometry);
    QVERIFY(fullPlan.shouldAttemptRestoreState);
    QVERIFY(fullPlan.shouldResizeToDefaultOnGeometryFailure);
}

void MainWindowSessionWindowRestoreServiceTest::decidesResizeFallback()
{
    const auto plan = MainWindowSessionWindowRestoreService::buildPlan(
        QByteArrayLiteral("geom"), QByteArray());

    QVERIFY(!MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(plan, true));
    QVERIFY(MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(plan, false));

    const auto noGeometryPlan = MainWindowSessionWindowRestoreService::buildPlan({}, QByteArray());
    QVERIFY(!MainWindowSessionWindowRestoreService::shouldResizeToDefaultAfterGeometryRestoreFailure(noGeometryPlan, false));
}
}

int runMainWindowSessionWindowRestoreServiceTest(int argc, char **argv)
{
    MainWindowSessionWindowRestoreServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowSessionWindowRestoreServiceTest.moc"
