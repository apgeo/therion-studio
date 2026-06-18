#include "../../src/core/ThreeDViewerSceneModel.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class ThreeDViewerSceneModelTest : public QObject
{
    Q_OBJECT

private slots:
    void buildsTherionOrderedSurveyPaths();
    void handlesMissingAndCyclicSurveyParents();
};

void ThreeDViewerSceneModelTest::buildsTherionOrderedSurveyPaths()
{
    ThreeDViewerSceneModel sceneModel;
    sceneModel.surveys.append(ThreeDViewerSurvey{1, 0, QStringLiteral("zadni_pole"), QString()});
    sceneModel.surveys.append(ThreeDViewerSurvey{2, 1, QStringLiteral("1318"), QString()});
    sceneModel.surveys.append(ThreeDViewerSurvey{3, 2, QStringLiteral("sifon_i"), QString()});
    sceneModel.surveys.append(ThreeDViewerSurvey{4, 3, QStringLiteral("velky_komin"), QString()});

    const ThreeDViewerStation station{1, 4, QStringLiteral("7"), QString(), {1.0, 2.0, 3.0}};
    const ThreeDViewerStation anonymousStation{2, 4, QString(), QString(), {1.0, 2.0, 3.0}};

    QCOMPARE(sceneModel.surveyPathForId(4), QStringLiteral("velky_komin.sifon_i.1318.zadni_pole"));
    QCOMPARE(sceneModel.stationQualifiedName(station), QStringLiteral("7@velky_komin.sifon_i.1318.zadni_pole"));
    QCOMPARE(sceneModel.stationQualifiedName(anonymousStation), QStringLiteral("@velky_komin.sifon_i.1318.zadni_pole"));
}

void ThreeDViewerSceneModelTest::handlesMissingAndCyclicSurveyParents()
{
    ThreeDViewerSceneModel sceneModel;
    sceneModel.surveys.append(ThreeDViewerSurvey{1, 2, QStringLiteral("child"), QString()});
    sceneModel.surveys.append(ThreeDViewerSurvey{2, 1, QStringLiteral("parent"), QString()});

    const ThreeDViewerStation cycleStation{1, 1, QStringLiteral("a"), QString(), {}};
    const ThreeDViewerStation missingSurveyStation{2, 99, QStringLiteral("b"), QString(), {}};

    QCOMPARE(sceneModel.surveyPathForId(1), QStringLiteral("child.parent"));
    QCOMPARE(sceneModel.stationQualifiedName(cycleStation), QStringLiteral("a@child.parent"));
    QCOMPARE(sceneModel.stationQualifiedName(missingSurveyStation), QStringLiteral("b"));
}

int runThreeDViewerSceneModelTest(int argc, char **argv)
{
    ThreeDViewerSceneModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerSceneModelTest.moc"
