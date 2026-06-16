#include "../src/app/MainWindowSessionProjectService.h"

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MainWindowSessionProjectServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void decidesMissingProjectRestore();
    void decidesRestorableProject();
    void detectsProtectedFolders();
};

void MainWindowSessionProjectServiceTest::decidesMissingProjectRestore()
{
    const auto emptyDecision = MainWindowSessionProjectService::decideProjectRestore(QString());
    QCOMPARE(emptyDecision.status, MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored);

    const auto missingDecision = MainWindowSessionProjectService::decideProjectRestore(QStringLiteral("/definitely/missing/path"));
    QCOMPARE(missingDecision.status, MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored);
}

void MainWindowSessionProjectServiceTest::decidesRestorableProject()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Temporary directory creation failed.");

    const auto decision = MainWindowSessionProjectService::decideProjectRestore(tempDir.path());
    QCOMPARE(decision.status, MainWindowSessionProjectService::ProjectRestoreStatus::Restored);
    QCOMPARE(decision.projectPath, tempDir.path());
}

void MainWindowSessionProjectServiceTest::detectsProtectedFolders()
{
    const QStringList protectedRoots = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)};

    QString protectedRoot;
    for (const QString &root : protectedRoots) {
        if (!root.isEmpty() && QDir(root).exists()) {
            protectedRoot = root;
            break;
        }
    }

    if (protectedRoot.isEmpty()) {
        QSKIP("No protected user folder exists on this test host.");
    }

    QVERIFY2(MainWindowSessionProjectService::isProtectedMacUserFolder(protectedRoot),
             "Known protected root should be detected as protected.");

    const QString childPath = QDir(protectedRoot).filePath(QStringLiteral("therion-studio-test-child"));
    QVERIFY2(MainWindowSessionProjectService::isProtectedMacUserFolder(childPath),
             "Path under a protected root should be detected as protected.");

    const auto decision = MainWindowSessionProjectService::decideProjectRestore(protectedRoot);
    QCOMPARE(decision.status, MainWindowSessionProjectService::ProjectRestoreStatus::SkippedProtectedFolder);
}
}

int runMainWindowSessionProjectServiceTest(int argc, char **argv)
{
    MainWindowSessionProjectServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MainWindowSessionProjectServiceTest.moc"
