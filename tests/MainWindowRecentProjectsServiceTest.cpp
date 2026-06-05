#include "../src/app/MainWindowRecentProjectsService.h"

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
        QDir::cleanPath(QStringLiteral("/tmp/beta")),
        QDir::cleanPath(QStringLiteral("/tmp/alpha")),
        QDir::cleanPath(QStringLiteral("/tmp/gamma"))};
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
        QDir::cleanPath(QStringLiteral("/tmp/six")),
        QDir::cleanPath(QStringLiteral("/tmp/one")),
        QDir::cleanPath(QStringLiteral("/tmp/two")),
        QDir::cleanPath(QStringLiteral("/tmp/three")),
        QDir::cleanPath(QStringLiteral("/tmp/four"))};
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
        QDir::cleanPath(QStringLiteral("/tmp/project")),
        QDir::cleanPath(QStringLiteral("/tmp/other"))};
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
