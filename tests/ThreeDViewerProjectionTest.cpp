#include "../src/app/three_d_viewer/ThreeDViewerProjection.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace TherionStudio;

class ThreeDViewerProjectionTest : public QObject
{
    Q_OBJECT

private slots:
    void projectsSceneCenterToViewportCenter();
    void rejectsPointsBehindCamera();
    void projectsLineEndpoints();
};

void ThreeDViewerProjectionTest::projectsSceneCenterToViewportCenter()
{
    ThreeDViewerCamera camera;
    camera.setState({{0.0, 0.0, 0.0}, 0.0, 0.0, 10.0});

    const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, {0.0, 0.0, 0.0}, 800, 600);

    QVERIFY(projected.visible);
    QCOMPARE(projected.screenPosition.x(), 400.0);
    QCOMPARE(projected.screenPosition.y(), 300.0);
    QVERIFY(projected.depth > 0.0);
}

void ThreeDViewerProjectionTest::rejectsPointsBehindCamera()
{
    ThreeDViewerCamera camera;
    camera.setState({{0.0, 0.0, 0.0}, 0.0, 0.0, 10.0});

    const ThreeDViewerProjectedPoint projected = ThreeDViewerProjection::projectPoint(camera, {20.0, 0.0, 0.0}, 800, 600);

    QVERIFY(!projected.visible);
}

void ThreeDViewerProjectionTest::projectsLineEndpoints()
{
    ThreeDViewerCamera camera;
    camera.setState({{0.0, 0.0, 0.0}, 0.0, 0.0, 10.0});

    QPointF fromScreen;
    QPointF toScreen;
    QVERIFY(ThreeDViewerProjection::projectLine(camera, {0.0, 0.0, -2.0}, {0.0, 0.0, 2.0}, 800, 600, &fromScreen, &toScreen));
    QVERIFY(fromScreen != toScreen);
    QVERIFY(std::abs(fromScreen.x() - 400.0) < 1e-6);
    QVERIFY(std::abs(toScreen.x() - 400.0) < 1e-6);
    QVERIFY(fromScreen.y() > toScreen.y());
}

int runThreeDViewerProjectionTest(int argc, char **argv)
{
    ThreeDViewerProjectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerProjectionTest.moc"
