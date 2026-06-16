#include "../../src/core/ThreeDViewerCamera.h"

#include <QtTest/QtTest>

#include <cmath>

using namespace TherionStudio;

class ThreeDViewerCameraTest : public QObject
{
    Q_OBJECT

private slots:
    void fitCentersAndScalesScene();
    void resetRestoresDefaultOrientation();
    void orbitPanAndZoomModifyState();
};

void ThreeDViewerCameraTest::fitCentersAndScalesScene()
{
    ThreeDViewerBounds bounds;
    bounds.include({-10.0, -20.0, -30.0});
    bounds.include({30.0, 20.0, 10.0});

    ThreeDViewerCamera camera;
    camera.fitToBounds(bounds);

    const ThreeDViewerCameraState state = camera.state();
    QCOMPARE(state.target.x, 10.0);
    QCOMPARE(state.target.y, 0.0);
    QCOMPARE(state.target.z, -10.0);
    QVERIFY(state.distance > 40.0);
}

void ThreeDViewerCameraTest::resetRestoresDefaultOrientation()
{
    ThreeDViewerBounds bounds;
    bounds.include({0.0, 0.0, 0.0});
    bounds.include({10.0, 10.0, 10.0});

    ThreeDViewerCamera camera;
    camera.setState({{1.0, 2.0, 3.0}, 0.3, -0.2, 7.0});
    camera.resetToBounds(bounds);

    const ThreeDViewerCameraState state = camera.state();
    QCOMPARE(state.target.x, 5.0);
    QCOMPARE(state.target.y, 5.0);
    QCOMPARE(state.target.z, 5.0);
    QCOMPARE(state.yaw, -0.85);
    QCOMPARE(state.pitch, 0.45);
    QVERIFY(state.distance > 0.0);
}

void ThreeDViewerCameraTest::orbitPanAndZoomModifyState()
{
    ThreeDViewerCamera camera;
    camera.setState({{0.0, 0.0, 0.0}, 0.0, 0.0, 100.0});

    camera.orbitByPixels(10.0, -20.0);
    ThreeDViewerCameraState state = camera.state();
    QVERIFY(std::abs(state.yaw - 0.08) < 1e-12);
    QVERIFY(std::abs(state.pitch + 0.16) < 1e-12);

    camera.zoomByFactor(0.5);
    state = camera.state();
    QCOMPARE(state.distance, 50.0);

    const ThreeDViewerVec3 beforePan = state.target;
    camera.panByPixels(12.0, -8.0, 100);
    state = camera.state();
    QVERIFY(state.target.x != beforePan.x || state.target.y != beforePan.y || state.target.z != beforePan.z);

    const double panScale = camera.screenPanScale(100);
    QVERIFY(panScale > 0.0);
}

int runThreeDViewerCameraTest(int argc, char **argv)
{
    ThreeDViewerCameraTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerCameraTest.moc"
