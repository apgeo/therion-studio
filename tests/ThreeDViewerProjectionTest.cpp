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
    void orthographicProjectionKeepsScaleAcrossDepth();
    void focalLengthChangesPerspectiveScale();
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

void ThreeDViewerProjectionTest::orthographicProjectionKeepsScaleAcrossDepth()
{
    ThreeDViewerCamera camera;
    camera.setState({{0.0, 0.0, 0.0}, 0.0, 0.0, 10.0});

    const ThreeDViewerProjectedPoint nearPoint = ThreeDViewerProjection::projectPoint(camera, {0.0, -1.0, 0.0}, 800, 600, true);
    const ThreeDViewerProjectedPoint farPoint = ThreeDViewerProjection::projectPoint(camera, {-5.0, -1.0, 0.0}, 800, 600, true);

    QVERIFY(nearPoint.visible);
    QVERIFY(farPoint.visible);
    QVERIFY(std::abs(nearPoint.screenPosition.y() - farPoint.screenPosition.y()) < 1e-6);
}

void ThreeDViewerProjectionTest::focalLengthChangesPerspectiveScale()
{
    ThreeDViewerCamera wideCamera;
    wideCamera.setState({{0.0, 0.0, 0.0}, 0.0, 0.0, 10.0, 20.0});
    ThreeDViewerCamera teleCamera;
    teleCamera.setState({{0.0, 0.0, 0.0}, 0.0, 0.0, 10.0, 70.0});

    const ThreeDViewerProjectedPoint widePoint = ThreeDViewerProjection::projectPoint(wideCamera, {0.0, 1.0, 0.0}, 800, 600);
    const ThreeDViewerProjectedPoint telePoint = ThreeDViewerProjection::projectPoint(teleCamera, {0.0, 1.0, 0.0}, 800, 600);

    QVERIFY(widePoint.visible);
    QVERIFY(telePoint.visible);
    QVERIFY(std::abs(telePoint.screenPosition.x() - 400.0) > std::abs(widePoint.screenPosition.x() - 400.0));
}

int runThreeDViewerProjectionTest(int argc, char **argv)
{
    ThreeDViewerProjectionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerProjectionTest.moc"
