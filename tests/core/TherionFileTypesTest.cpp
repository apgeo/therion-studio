#include "../../src/core/TherionFileTypes.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

class TherionFileTypesTest : public QObject
{
    Q_OBJECT

private slots:
    void recognizesThreeDViewerArtifacts();
    void keepsTherionProjectFileRecognition();
};

void TherionFileTypesTest::recognizesThreeDViewerArtifacts()
{
    QVERIFY(isThreeDViewerArtifactFileName(QStringLiteral("scene.LOX")));
    QVERIFY(isThreeDViewerArtifactFilePath(QStringLiteral("/tmp/example.lox")));
    QVERIFY(!isThreeDViewerArtifactFilePath(QStringLiteral("/tmp/example.th2")));
}

void TherionFileTypesTest::keepsTherionProjectFileRecognition()
{
    QVERIFY(isTherionProjectFilePath(QStringLiteral("/tmp/project.th")));
    QVERIFY(isTherionProjectFilePath(QStringLiteral("/tmp/project.th2")));
    QVERIFY(isTherionProjectFilePath(QStringLiteral("/tmp/thconfig")));
    QVERIFY(!isTherionProjectFilePath(QStringLiteral("/tmp/example.lox")));
}

int runTherionFileTypesTest(int argc, char **argv)
{
    TherionFileTypesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionFileTypesTest.moc"
