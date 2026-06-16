#include "../src/app/TherionExecutableSelectionController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionExecutableSelectionControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void computesInitialBrowsePath();
    void rejectsEmptySelection();
    void rejectsNonExecutableSelection();
    void acceptsExecutableSelection();
};

bool writeTextFile(const QString &filePath, const QString &contents = QString())
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents.toUtf8()) == contents.toUtf8().size();
}

void TherionExecutableSelectionControllerTest::computesInitialBrowsePath()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("candidate.txt"));
    QVERIFY2(writeTextFile(filePath), "Temporary browse candidate could not be written.");

    QCOMPARE(TherionExecutableSelectionController::initialBrowsePath(QString()), QDir::homePath());
    QCOMPARE(TherionExecutableSelectionController::initialBrowsePath(QDir(tempDir.path()).filePath(QStringLiteral("missing"))),
             QDir::homePath());
    QCOMPARE(TherionExecutableSelectionController::initialBrowsePath(filePath), tempDir.path());
    QCOMPARE(TherionExecutableSelectionController::initialBrowsePath(tempDir.path()), tempDir.path());
}

void TherionExecutableSelectionControllerTest::rejectsEmptySelection()
{
    const TherionExecutableSelectionController::SelectionResult result =
        TherionExecutableSelectionController::evaluateSelection(QString());

    QVERIFY(!result.isAccepted);
    QVERIFY(!result.showWarningDialog);
    QVERIFY(!result.shouldUpdateExecutableText);
}

void TherionExecutableSelectionControllerTest::rejectsNonExecutableSelection()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("not-executable.txt"));
    QVERIFY2(writeTextFile(filePath, QStringLiteral("not executable\n")),
             "Temporary non-executable candidate could not be written.");
    QVERIFY2(!QFileInfo(filePath).isExecutable(),
             "Temporary text fixture unexpectedly appears executable on this platform.");

    const TherionExecutableSelectionController::SelectionResult result =
        TherionExecutableSelectionController::evaluateSelection(filePath);

    QVERIFY(!result.isAccepted);
    QVERIFY(result.showWarningDialog);
    QCOMPARE(result.warningDialogTitle, QStringLiteral("Select Therion Executable"));
    QCOMPARE(result.warningDialogMessage, QStringLiteral("The selected file is not executable."));
}

void TherionExecutableSelectionControllerTest::acceptsExecutableSelection()
{
    const QString applicationPath = QCoreApplication::applicationFilePath();
    const QFileInfo applicationInfo(applicationPath);
    QVERIFY2(applicationInfo.isExecutable(), "The running test binary should be executable.");

    const TherionExecutableSelectionController::SelectionResult result =
        TherionExecutableSelectionController::evaluateSelection(applicationPath);

    QVERIFY(result.isAccepted);
    QVERIFY(result.shouldUpdateExecutableText);
    QCOMPARE(result.updatedExecutableText, applicationInfo.absoluteFilePath());
    QVERIFY(result.shouldShowStatusBarMessage);
    QCOMPARE(result.statusBarMessage, QStringLiteral("Therion executable updated"));
    QCOMPARE(result.statusBarTimeoutMs, 2000);
}
}

int runTherionExecutableSelectionControllerTest(int argc, char **argv)
{
    TherionExecutableSelectionControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionExecutableSelectionControllerTest.moc"
