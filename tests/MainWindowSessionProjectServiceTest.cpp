#include "../src/app/MainWindowSessionProjectService.h"

#include <QDir>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runMissingProjectRestoreDecisionTest()
{
    const auto emptyDecision = MainWindowSessionProjectService::decideProjectRestore(QString());
    if (!expect(emptyDecision.status == MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored,
                "Empty project path should not restore any project.")) {
        return 1;
    }

    const auto missingDecision = MainWindowSessionProjectService::decideProjectRestore(QStringLiteral("/definitely/missing/path"));
    if (!expect(missingDecision.status == MainWindowSessionProjectService::ProjectRestoreStatus::NotRestored,
                "Missing project path should not restore any project.")) {
        return 1;
    }

    return 0;
}

int runRestorableProjectDecisionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const auto decision = MainWindowSessionProjectService::decideProjectRestore(tempDir.path());
    if (!expect(decision.status == MainWindowSessionProjectService::ProjectRestoreStatus::Restored,
                "Existing project path outside protected roots should be restorable.")) {
        return 1;
    }
    if (!expect(decision.projectPath == tempDir.path(),
                "Restorable decision should preserve the input project path.")) {
        return 1;
    }

    return 0;
}

int runProtectedFolderDetectionTest()
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
        return 0;
    }

    if (!expect(MainWindowSessionProjectService::isProtectedMacUserFolder(protectedRoot),
                "Known protected root should be detected as protected.")) {
        return 1;
    }

    const QString childPath = QDir(protectedRoot).filePath(QStringLiteral("therion-studio-test-child"));
    if (!expect(MainWindowSessionProjectService::isProtectedMacUserFolder(childPath),
                "Path under a protected root should be detected as protected.")) {
        return 1;
    }

    const auto decision = MainWindowSessionProjectService::decideProjectRestore(protectedRoot);
    if (!expect(decision.status == MainWindowSessionProjectService::ProjectRestoreStatus::SkippedProtectedFolder,
                "Protected folder should be skipped during automatic restore.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runMissingProjectRestoreDecisionTest() != 0) {
        return 1;
    }
    if (runRestorableProjectDecisionTest() != 0) {
        return 1;
    }
    if (runProtectedFolderDetectionTest() != 0) {
        return 1;
    }

    return 0;
}
