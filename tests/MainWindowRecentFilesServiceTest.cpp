#include "../src/app/MainWindowRecentFilesService.h"

#include <QCoreApplication>
#include <QStringList>

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

int runProjectScopedNormalizationTest()
{
    const QString projectPath = QStringLiteral("/tmp/project");
    const QStringList normalizedPaths =
        MainWindowRecentFilesService::normalizedRecentFilePaths(
            projectPath,
            {QString(),
             QStringLiteral("/tmp/project/a.th"),
             QStringLiteral("/tmp/project/a.th"),
             QStringLiteral("/tmp/project/maps/map.th2"),
             QStringLiteral("/tmp/other/outside.th")});

    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/a.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/maps/map.th2"))};
    if (!expect(normalizedPaths == expectedPaths,
                "Recent file normalization should deduplicate and keep only files inside the project.")) {
        return 1;
    }

    return 0;
}

int runRecordOpenedFileTest()
{
    const QString projectPath = QStringLiteral("/tmp/project");
    const QStringList currentPaths = {
        QStringLiteral("/tmp/project/a.th"),
        QStringLiteral("/tmp/project/b.th"),
        QStringLiteral("/tmp/project/c.th2")};

    const QStringList updatedPaths =
        MainWindowRecentFilesService::recordOpenedFile(projectPath,
                                                       currentPaths,
                                                       QStringLiteral("/tmp/project/b.th"));
    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/b.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/a.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/c.th2"))};
    if (!expect(updatedPaths == expectedPaths,
                "Opening an existing recent file should move it to the front without duplicates.")) {
        return 1;
    }

    return 0;
}

int runMaxRecentFileCountTest()
{
    const QString projectPath = QStringLiteral("/tmp/project");
    const QStringList currentPaths = {
        QStringLiteral("/tmp/project/one.th"),
        QStringLiteral("/tmp/project/two.th"),
        QStringLiteral("/tmp/project/three.th"),
        QStringLiteral("/tmp/project/four.th"),
        QStringLiteral("/tmp/project/five.th"),
        QStringLiteral("/tmp/project/six.th"),
        QStringLiteral("/tmp/project/seven.th"),
        QStringLiteral("/tmp/project/eight.th"),
        QStringLiteral("/tmp/project/nine.th"),
        QStringLiteral("/tmp/project/ten.th")};

    const QStringList updatedPaths =
        MainWindowRecentFilesService::recordOpenedFile(projectPath,
                                                       currentPaths,
                                                       QStringLiteral("/tmp/project/eleven.th"));
    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/eleven.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/one.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/two.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/three.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/four.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/five.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/six.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/seven.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/eight.th")),
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/nine.th"))};
    if (!expect(updatedPaths == expectedPaths,
                "Recent file history should keep only the ten newest entries.")) {
        return 1;
    }

    return 0;
}

int runOutsideProjectRejectedTest()
{
    const QStringList updatedPaths =
        MainWindowRecentFilesService::recordOpenedFile(QStringLiteral("/tmp/project"),
                                                       {QStringLiteral("/tmp/project/a.th")},
                                                       QStringLiteral("/tmp/outside.th"));
    const QStringList expectedPaths = {
        MainWindowRecentFilesService::normalizedFilePath(QStringLiteral("/tmp/project/a.th"))};
    if (!expect(updatedPaths == expectedPaths,
                "Opening a file outside the project should not enter project-scoped recent files.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runProjectScopedNormalizationTest() != 0) {
        return 1;
    }
    if (runRecordOpenedFileTest() != 0) {
        return 1;
    }
    if (runMaxRecentFileCountTest() != 0) {
        return 1;
    }
    if (runOutsideProjectRejectedTest() != 0) {
        return 1;
    }

    return 0;
}
