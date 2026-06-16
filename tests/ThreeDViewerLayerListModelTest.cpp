#include "../src/app/three_d_viewer/ThreeDViewerLayerListModel.h"

#include <QtTest/QtTest>
#include <QSignalSpy>

using namespace TherionStudio;

class ThreeDViewerLayerListModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesLayerCountsAndVisibility();
};

void ThreeDViewerLayerListModelTest::exposesLayerCountsAndVisibility()
{
    ThreeDViewerSceneModel sceneModel;

    sceneModel.shots.append(ThreeDViewerShot{});
    sceneModel.shots.append(ThreeDViewerShot{});
    sceneModel.stations.append(ThreeDViewerStation{});
    sceneModel.meshGroups.append(ThreeDViewerMeshGroup{});
    sceneModel.surfaces.append(ThreeDViewerSurface{});

    ThreeDViewerLayerListModel model;
    model.setSceneModel(sceneModel);

    QCOMPARE(model.rowCount(), 5);

    const QModelIndex centerlineIndex = model.index(0, 0);
    QCOMPARE(model.data(centerlineIndex, Qt::DisplayRole).toString(), QStringLiteral("Centerline (2)"));
    QCOMPARE(model.data(centerlineIndex, Qt::CheckStateRole).toInt(), int(Qt::Checked));

    QSignalSpy spy(&model, &ThreeDViewerLayerListModel::layerVisibilityChanged);
    QVERIFY(model.setData(centerlineIndex, Qt::Unchecked, Qt::CheckStateRole));
    QCOMPARE(model.data(centerlineIndex, Qt::CheckStateRole).toInt(), int(Qt::Unchecked));
    QCOMPARE(spy.count(), 1);
}

int runThreeDViewerLayerListModelTest(int argc, char **argv)
{
    ThreeDViewerLayerListModelTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerLayerListModelTest.moc"
