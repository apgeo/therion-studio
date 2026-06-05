#include "../src/app/MainWindowRecentProjectsService.h"

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

int runRecordOpenedProjectTest()
{
    const QStringList currentPaths = {
        QStringLiteral("/tmp/alpha"),
        QStringLiteral("/tmp/beta"),
        QStringLiteral("/tmp/gamma")};

    const QStringList updatedPaths =
        MainWindowRecentProjectsService::recordOpenedProject(currentPaths,
                                                             QStringLiteral("/tmp/beta"));

    const QStringList expectedPaths = {
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/beta")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/alpha")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/gamma"))};
    if (!expect(updatedPaths == expectedPaths,
                "Opening an existing recent project should move it to the front without duplicates.")) {
        return 1;
    }

    return 0;
}

int runMaxRecentProjectCountTest()
{
    const QStringList currentPaths = {
        QStringLiteral("/tmp/one"),
        QStringLiteral("/tmp/two"),
        QStringLiteral("/tmp/three"),
        QStringLiteral("/tmp/four"),
        QStringLiteral("/tmp/five")};

    const QStringList updatedPaths =
        MainWindowRecentProjectsService::recordOpenedProject(currentPaths,
                                                             QStringLiteral("/tmp/six"));

    const QStringList expectedPaths = {
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/six")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/one")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/two")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/three")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/four"))};
    if (!expect(updatedPaths == expectedPaths,
                "Recent project history should keep only the five newest entries.")) {
        return 1;
    }

    return 0;
}

int runNormalizationTest()
{
    const QStringList normalizedPaths =
        MainWindowRecentProjectsService::normalizedRecentProjectPaths({
            QString(),
            QStringLiteral("  /tmp/project/../project  "),
            QStringLiteral("/tmp/project"),
            QStringLiteral("/tmp/other")});

    const QStringList expectedPaths = {
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/project")),
        MainWindowRecentProjectsService::normalizedProjectPath(QStringLiteral("/tmp/other"))};
    if (!expect(normalizedPaths == expectedPaths,
                "Recent project normalization should trim, absolutize, clean, and deduplicate paths.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runRecordOpenedProjectTest() != 0) {
        return 1;
    }
    if (runMaxRecentProjectCountTest() != 0) {
        return 1;
    }
    if (runNormalizationTest() != 0) {
        return 1;
    }

    return 0;
}
