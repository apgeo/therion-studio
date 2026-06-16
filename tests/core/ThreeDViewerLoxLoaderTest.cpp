#include "../../src/core/ThreeDViewerLoxLoader.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
QString repositoryRoot()
{
    QDir dir(QCoreApplication::applicationDirPath());
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

class ThreeDViewerLoxLoaderTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsSampleLoxScene();
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
