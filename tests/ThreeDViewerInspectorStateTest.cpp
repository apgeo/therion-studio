#include "../src/app/three_d_viewer/ThreeDViewerInspectorState.h"

#include <QtTest/QtTest>
#include <QLocale>

using namespace TherionStudio;

class ThreeDViewerInspectorStateTest : public QObject
{
    Q_OBJECT

private slots:
    void tracksFilePathAndSceneMetrics();
    void tracksMeshColorMode();
    void tracksMeasurementMode();
    void tracksAutoRotationSettings();
};

void ThreeDViewerInspectorStateTest::tracksFilePathAndSceneMetrics()
{
    ThreeDViewerInspectorState state;

    QCOMPARE(state.filePath(), QString());
    QCOMPARE(state.meshColorMode(), int(ThreeDViewerMeshColorMode::Survey));
    QCOMPARE(state.measurementMode(), false);
    QCOMPARE(state.caveLengthText(), QLocale::system().toString(0.0, 'f', 1) + QStringLiteral(" m"));
    QCOMPARE(state.caveDepthText(), QLocale::system().toString(0.0, 'f', 1) + QStringLiteral(" m"));
    QCOMPARE(state.surveyCount(), 0);
    QCOMPARE(state.stationCount(), 0);
    QCOMPARE(state.shotCount(), 0);
    QCOMPARE(state.meshCount(), 0);
    QCOMPARE(state.surfaceCount(), 0);

    ThreeDViewerSceneModel sceneModel;
    sceneModel.surveys.append(ThreeDViewerSurvey{1, 0, QStringLiteral("survey"), QStringLiteral("Survey")});
    sceneModel.stations.append(ThreeDViewerStation{1, 1, QStringLiteral("a"), QString(), {0.0, 0.0, 0.0}});
    sceneModel.stations.append(ThreeDViewerStation{2, 1, QStringLiteral("b"), QString(), {6.0, 8.0, -10.0}});
    sceneModel.shots.append(ThreeDViewerShot{1, 2});
    sceneModel.meshGroups.append(ThreeDViewerMeshGroup{});
    sceneModel.surfaces.append(ThreeDViewerSurface{});

    state.setFilePath(QStringLiteral("/tmp/example.lox"));
    state.setSceneModel(sceneModel);

    QCOMPARE(state.filePath(), QStringLiteral("/tmp/example.lox"));
    QCOMPARE(state.caveLengthText(), QStringLiteral("14 m"));
    QCOMPARE(state.caveDepthText(), QStringLiteral("10 m"));
    QCOMPARE(state.surveyCount(), 1);
    QCOMPARE(state.stationCount(), 2);
    QCOMPARE(state.shotCount(), 1);
    QCOMPARE(state.meshCount(), 1);
    QCOMPARE(state.surfaceCount(), 1);
}

void ThreeDViewerInspectorStateTest::tracksMeshColorMode()
{
    ThreeDViewerInspectorState state;
    QSignalSpy spy(&state, &ThreeDViewerInspectorState::meshColorModeChanged);

    state.setMeshColorMode(int(ThreeDViewerMeshColorMode::Depth));

    QCOMPARE(state.meshColorMode(), int(ThreeDViewerMeshColorMode::Depth));
    QCOMPARE(spy.count(), 1);
}

void ThreeDViewerInspectorStateTest::tracksMeasurementMode()
{
    ThreeDViewerInspectorState state;
    QSignalSpy spy(&state, &ThreeDViewerInspectorState::measurementModeChanged);

    state.setMeasurementMode(true);

    QCOMPARE(state.measurementMode(), true);
    QCOMPARE(spy.count(), 1);
}

void ThreeDViewerInspectorStateTest::tracksAutoRotationSettings()
{
    ThreeDViewerInspectorState state;
    QSignalSpy enabledSpy(&state, &ThreeDViewerInspectorState::autoRotationEnabledChanged);
    QSignalSpy speedSpy(&state, &ThreeDViewerInspectorState::autoRotationSpeedChanged);

    QCOMPARE(state.autoRotationEnabled(), false);
    QCOMPARE(state.autoRotationSpeed(), 30.0);

    state.setAutoRotationEnabled(true);
    state.setAutoRotationSpeed(45.0);

    QCOMPARE(state.autoRotationEnabled(), true);
    QCOMPARE(state.autoRotationSpeed(), 45.0);
    QCOMPARE(enabledSpy.count(), 1);
    QCOMPARE(speedSpy.count(), 1);

    state.setAutoRotationSpeed(0.0);
    QCOMPARE(state.autoRotationSpeed(), 5.0);

    state.setAutoRotationSpeed(360.0);
    QCOMPARE(state.autoRotationSpeed(), 90.0);
}

int runThreeDViewerInspectorStateTest(int argc, char **argv)
{
    ThreeDViewerInspectorStateTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerInspectorStateTest.moc"
