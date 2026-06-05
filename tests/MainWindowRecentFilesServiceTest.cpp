#include "../src/app/MainWindowRecentFilesService.h"

#include <QCoreApplication>
#include <QDir>
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
        QDir::cleanPath(QStringLiteral("/tmp/project/a.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/maps/map.th2"))};
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
        QDir::cleanPath(QStringLiteral("/tmp/project/b.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/a.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/c.th2"))};
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
        QDir::cleanPath(QStringLiteral("/tmp/project/eleven.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/one.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/two.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/three.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/four.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/five.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/six.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/seven.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/eight.th")),
        QDir::cleanPath(QStringLiteral("/tmp/project/nine.th"))};
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
        QDir::cleanPath(QStringLiteral("/tmp/project/a.th"))};
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
