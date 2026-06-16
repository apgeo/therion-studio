#include "../src/app/three_d_viewer/ThreeDViewerViewportController.h"

#include <QtTest/QtTest>
#include <QSignalSpy>

#include <cmath>

using namespace TherionStudio;

class ThreeDViewerViewportControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void rotatesAroundBlueAxisAndZooms();
    void orbitsAroundScene();
    void emitsCameraChanged();
};

void ThreeDViewerViewportControllerTest::rotatesAroundBlueAxisAndZooms()
{
    ThreeDViewerViewportController controller;

    const ThreeDViewerCameraState beforeRotate = controller.camera().state();
    controller.rotateLeft();
    const ThreeDViewerCameraState afterRotate = controller.camera().state();
    QVERIFY(std::abs(afterRotate.yaw - (beforeRotate.yaw + 3.14159265358979323846 / 12.0)) < 1e-12);
    QCOMPARE(afterRotate.pitch, beforeRotate.pitch);
    QCOMPARE(afterRotate.distance, beforeRotate.distance);
    QCOMPARE(afterRotate.target.x, beforeRotate.target.x);
    QCOMPARE(afterRotate.target.y, beforeRotate.target.y);
    QCOMPARE(afterRotate.target.z, beforeRotate.target.z);

    const ThreeDViewerCameraState beforeZoom = controller.camera().state();
    QVERIFY(controller.wheel(QPoint(0, 120)));
    const ThreeDViewerCameraState afterZoom = controller.camera().state();
    QVERIFY(afterZoom.distance < beforeZoom.distance);
}

void ThreeDViewerViewportControllerTest::orbitsAroundScene()
{
    ThreeDViewerViewportController controller;

    QVERIFY(controller.mousePress(Qt::LeftButton, QPoint(10, 10)));
    QVERIFY(controller.mouseMove(QPoint(30, 45), 400));
    const ThreeDViewerCameraState orbitState = controller.camera().state();
    QVERIFY(std::abs(orbitState.yaw - (-0.85 + 20.0 * 0.008)) < 1e-12);
    QVERIFY(std::abs(orbitState.pitch - (0.45 + 35.0 * 0.008)) < 1e-12);
    QVERIFY(controller.mouseRelease(Qt::LeftButton));
}

void ThreeDViewerViewportControllerTest::emitsCameraChanged()
{
    ThreeDViewerViewportController controller;
    QSignalSpy spy(&controller, &ThreeDViewerViewportController::cameraChanged);

    controller.rotateLeft();
    controller.fitToScene(ThreeDViewerSceneModel{});
    controller.wheel(QPoint(0, 120));

    QCOMPARE(spy.count(), 3);
}

int runThreeDViewerViewportControllerTest(int argc, char **argv)
{
    ThreeDViewerViewportControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerViewportControllerTest.moc"
