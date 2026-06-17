#include "../src/app/three_d_viewer/ThreeDViewerInspectorState.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class ThreeDViewerInspectorStateTest : public QObject
{
    Q_OBJECT

private slots:
    void tracksFilePathAndSceneCounts();
    void tracksMeshColorMode();
    void tracksMeasurementMode();
};

void ThreeDViewerInspectorStateTest::tracksFilePathAndSceneCounts()
{
    ThreeDViewerInspectorState state;

    QCOMPARE(state.filePath(), QString());
    QCOMPARE(state.meshColorMode(), int(ThreeDViewerMeshColorMode::Survey));
    QCOMPARE(state.measurementMode(), false);
    QCOMPARE(state.surveyCount(), 0);
    QCOMPARE(state.stationCount(), 0);
    QCOMPARE(state.shotCount(), 0);
    QCOMPARE(state.meshCount(), 0);
    QCOMPARE(state.surfaceCount(), 0);

    ThreeDViewerSceneModel sceneModel;
    sceneModel.surveys.append(ThreeDViewerSurvey{});
    sceneModel.stations.append(ThreeDViewerStation{});
    sceneModel.shots.append(ThreeDViewerShot{});
    sceneModel.meshGroups.append(ThreeDViewerMeshGroup{});
    sceneModel.surfaces.append(ThreeDViewerSurface{});

    state.setFilePath(QStringLiteral("/tmp/example.lox"));
    state.setSceneModel(sceneModel);

    QCOMPARE(state.filePath(), QStringLiteral("/tmp/example.lox"));
    QCOMPARE(state.surveyCount(), 1);
    QCOMPARE(state.stationCount(), 1);
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

int runThreeDViewerInspectorStateTest(int argc, char **argv)
{
    ThreeDViewerInspectorStateTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerInspectorStateTest.moc"
