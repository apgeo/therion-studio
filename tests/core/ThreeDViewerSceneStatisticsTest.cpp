#include "../../src/core/ThreeDViewerSceneStatistics.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class ThreeDViewerSceneStatisticsTest : public QObject
{
    Q_OBJECT

private slots:
    void computesCaveLengthAndDepthFromUndergroundCenterlineOnly();
};

void ThreeDViewerSceneStatisticsTest::computesCaveLengthAndDepthFromUndergroundCenterlineOnly()
{
    ThreeDViewerSceneModel sceneModel;
    sceneModel.surveys.append(ThreeDViewerSurvey{1, 0, QStringLiteral("zadni_pole"), QStringLiteral("Závrtová skupina Zadní pole")});
    sceneModel.stations.append(ThreeDViewerStation{1, 1, QStringLiteral("a"), QString(), {0.0, 0.0, 0.0}});
    sceneModel.stations.append(ThreeDViewerStation{2, 1, QStringLiteral("b"), QString(), {0.0, 0.0, -10.0}});
    sceneModel.stations.append(ThreeDViewerStation{3, 1, QStringLiteral("c"), QString(), {100.0, 100.0, 50.0}});
    sceneModel.stations.append(ThreeDViewerStation{4, 1, QStringLiteral("d"), QString(), {20.0, -10.0, -40.0}});
    sceneModel.stations.append(ThreeDViewerStation{5, 1, QStringLiteral("e"), QString(), {5.0, 5.0, -100.0}});

    sceneModel.shots.append(ThreeDViewerShot{1, 2});
    sceneModel.shots.append(ThreeDViewerShot{2, 3, 0, {}, {}, 0.0, true});
    sceneModel.shots.append(ThreeDViewerShot{2, 4, 0, {}, {}, 0.0, false, true});
    sceneModel.shots.append(ThreeDViewerShot{2, 5, 0, {}, {}, 0.0, false, false, false, false, true});

    const ThreeDViewerSceneStatistics statistics = computeThreeDViewerSceneStatistics(sceneModel);

    QCOMPARE(statistics.sceneTitle, QStringLiteral("Závrtová skupina Zadní pole"));
    QCOMPARE(statistics.caveLengthMeters, 10.0);
    QCOMPARE(statistics.caveDepthMeters, 10.0);
}

int runThreeDViewerSceneStatisticsTest(int argc, char **argv)
{
    ThreeDViewerSceneStatisticsTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerSceneStatisticsTest.moc"
