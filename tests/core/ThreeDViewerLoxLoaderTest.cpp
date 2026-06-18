#include "../../src/core/ThreeDViewerLoxLoader.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
QString repositoryRoot()
{
    QDir dir(QDir::currentPath());
    while (!dir.isRoot()) {
        if (QFile::exists(dir.filePath(QStringLiteral("CMakeLists.txt")))
            && QFile::exists(dir.filePath(QStringLiteral("sample_data")))) {
            return dir.absolutePath();
        }
        dir.cdUp();
    }
    return {};
}

QString sampleLoxPath()
{
    const QString root = repositoryRoot();
    if (root.isEmpty()) {
        return {};
    }
    return QDir(root).filePath(
        QStringLiteral("sample_data/babice/02_clopy_a_okoli/1302_ve_clopech/_output/1302.lox"));
}

QStringList sampleLoxPaths()
{
    const QString root = repositoryRoot();
    if (root.isEmpty()) {
        return {};
    }

    QStringList paths;
    QDirIterator it(QDir(root).filePath(QStringLiteral("sample_data")),
                    QStringList{QStringLiteral("*.lox")},
                    QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        paths.append(it.next());
    }
    paths.sort();
    return paths;
}

class ThreeDViewerLoxLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsSampleLoxScene();
    void loadsSampleLoxFixtureMatrix_data();
    void loadsSampleLoxFixtureMatrix();
    void sampleFixtureMatrixCoversSceneSemantics();
    void rejectsEmptyInput();
    void rejectsTruncatedInput();
};

void ThreeDViewerLoxLoaderTest::loadsSampleLoxScene()
{
    const QString path = sampleLoxPath();
    QVERIFY2(!path.isEmpty(), "Repository root with sample_data was not found.");
    QVERIFY2(QFile::exists(path), qPrintable(QStringLiteral("Sample .lox fixture is missing: %1").arg(path)));

    const ThreeDViewerLoxLoader loader;
    const ThreeDViewerLoxLoader::Result result = loader.loadFile(path);

    QVERIFY2(result.ok(), qPrintable(result.error));
    QVERIFY(!result.scene.isEmpty());
    QVERIFY(result.scene.surveys.size() > 0);
    QVERIFY(result.scene.stations.size() > 0);
    QVERIFY(result.scene.shots.size() > 0);

    const ThreeDViewerBounds bounds = result.scene.bounds();
    QVERIFY(bounds.valid);
    QVERIFY(bounds.minimum.x <= bounds.maximum.x);
    QVERIFY(bounds.minimum.y <= bounds.maximum.y);
    QVERIFY(bounds.minimum.z <= bounds.maximum.z);

    bool hasNamedStation = false;
    for (const ThreeDViewerStation &station : result.scene.stations) {
        if (!station.name.isEmpty()) {
            hasNamedStation = true;
            break;
        }
    }
    QVERIFY(hasNamedStation);
}

void ThreeDViewerLoxLoaderTest::loadsSampleLoxFixtureMatrix_data()
{
    QTest::addColumn<QString>("path");

    const QStringList paths = sampleLoxPaths();
    QVERIFY2(!paths.isEmpty(), "No sample .lox fixtures were found.");
    for (const QString &path : paths) {
        QTest::newRow(qPrintable(QFileInfo(path).fileName())) << path;
    }
}

void ThreeDViewerLoxLoaderTest::loadsSampleLoxFixtureMatrix()
{
    QFETCH(QString, path);

    const ThreeDViewerLoxLoader loader;
    const ThreeDViewerLoxLoader::Result result = loader.loadFile(path);

    QVERIFY2(result.ok(), qPrintable(QStringLiteral("%1: %2").arg(path, result.error)));
    QVERIFY2(!result.scene.isEmpty(), qPrintable(path));
    QVERIFY2(result.scene.bounds().valid, qPrintable(path));
    QVERIFY2(!result.scene.surveys.isEmpty(), qPrintable(path));
    QVERIFY2(!result.scene.stations.isEmpty(), qPrintable(path));
}

void ThreeDViewerLoxLoaderTest::sampleFixtureMatrixCoversSceneSemantics()
{
    const QStringList paths = sampleLoxPaths();
    QVERIFY2(!paths.isEmpty(), "No sample .lox fixtures were found.");

    bool hasNestedSurvey = false;
    bool hasMesh = false;
    bool hasSurfaceShot = false;
    bool hasDuplicateShot = false;
    bool hasSplayShot = false;
    bool hasStationFlag = false;

    const ThreeDViewerLoxLoader loader;
    for (const QString &path : paths) {
        const ThreeDViewerLoxLoader::Result result = loader.loadFile(path);
        QVERIFY2(result.ok(), qPrintable(QStringLiteral("%1: %2").arg(path, result.error)));

        for (const ThreeDViewerSurvey &survey : result.scene.surveys) {
            hasNestedSurvey = hasNestedSurvey || survey.parentId != 0;
        }
        hasMesh = hasMesh || !result.scene.meshGroups.isEmpty();
        for (const ThreeDViewerShot &shot : result.scene.shots) {
            hasSurfaceShot = hasSurfaceShot || shot.surface;
            hasDuplicateShot = hasDuplicateShot || shot.duplicate;
            hasSplayShot = hasSplayShot || shot.splay;
        }
        for (const ThreeDViewerStation &station : result.scene.stations) {
            hasStationFlag = hasStationFlag || station.surface || station.entrance || station.fixed
                || station.continuation || station.hasWalls;
        }
    }

    QVERIFY(hasNestedSurvey);
    QVERIFY(hasMesh);
    QVERIFY(hasSurfaceShot);
    QVERIFY(hasDuplicateShot);
    QVERIFY(hasSplayShot);
    QVERIFY(hasStationFlag);
}

void ThreeDViewerLoxLoaderTest::rejectsEmptyInput()
{
    const ThreeDViewerLoxLoader loader;
    const ThreeDViewerLoxLoader::Result result = loader.loadBytes({});

    QVERIFY(!result.ok());
    QVERIFY(result.error.contains(QStringLiteral("empty")));
}

void ThreeDViewerLoxLoaderTest::rejectsTruncatedInput()
{
    const ThreeDViewerLoxLoader loader;
    const QByteArray truncatedHeader(8, '\0');
    const ThreeDViewerLoxLoader::Result result = loader.loadBytes(truncatedHeader);

    QVERIFY(!result.ok());
    QVERIFY(result.error.contains(QStringLiteral("truncated")));
}
}

int runThreeDViewerLoxLoaderTest(int argc, char **argv)
{
    ThreeDViewerLoxLoaderTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "ThreeDViewerLoxLoaderTest.moc"
