#include "../src/app/TherionRunnerConfigDisplayController.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionRunnerConfigDisplayControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void showsNoConfigForEmptyResolvedPath();
    void buildsResolvedConfigDisplayFields();
};

void TherionRunnerConfigDisplayControllerTest::showsNoConfigForEmptyResolvedPath()
{
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(QString());

    QVERIFY(!result.hasResolvedConfigPath);
    QVERIFY(result.configName.isEmpty());
    QVERIFY(result.configPath.isEmpty());
    QVERIFY(result.configDirectory.isEmpty());
}

void TherionRunnerConfigDisplayControllerTest::buildsResolvedConfigDisplayFields()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("custom.thconfig"));
    const TherionRunnerConfigDisplayController::RefreshComputation result =
        TherionRunnerConfigDisplayController::compute(configPath);

    QVERIFY(result.hasResolvedConfigPath);
    QCOMPARE(result.configName, QStringLiteral("custom.thconfig"));
    QCOMPARE(result.configPath, configPath);
    QCOMPARE(result.configDirectory, tempDir.path());
}
}

int runTherionRunnerConfigDisplayControllerTest(int argc, char **argv)
{
    TherionRunnerConfigDisplayControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionRunnerConfigDisplayControllerTest.moc"
