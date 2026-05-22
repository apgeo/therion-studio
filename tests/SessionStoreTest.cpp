#include "../src/core/SessionStore.h"

#include <QCoreApplication>
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

int runInstanceBackedRoundTripTest()
{
    QTemporaryDir temporarySettingsDir;
    if (!expect(temporarySettingsDir.isValid(), "Temporary settings directory creation failed.")) {
        return 1;
    }

    const QString settingsPath = temporarySettingsDir.filePath(QStringLiteral("session.ini"));

    {
        SessionSettingsStore store(settingsPath);
        store.setLastProjectPath(QStringLiteral("/tmp/project"));
        store.setMainWindowGeometry(QByteArray("geometry-bytes"));
        store.setMainWindowState(QByteArray("state-bytes"));
        store.setOpenDocumentPaths({QStringLiteral("/tmp/project/a.th"), QStringLiteral("/tmp/project/b.th2")});
        store.setActiveDocumentPath(QStringLiteral("/tmp/project/b.th2"));
        store.setStructureNameOverrides(QStringLiteral("{\"a\":\"A\"}"));
        store.setTherionExecutablePath(QStringLiteral("/usr/bin/therion"));
        store.setTherionWorkingDirectory(QStringLiteral("/tmp/project"));
        store.setTherionArguments(QStringLiteral("-q thconfig"));
        store.setTherionMapTouchFriendlyControlsEnabled(true);
        store.setTherionMapBackgroundLayers(QStringLiteral("[]"));
    }

    const SessionSettingsStore store(settingsPath);
    if (!expect(store.lastProjectPath() == QStringLiteral("/tmp/project"),
                "Last project path should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.mainWindowGeometry() == QByteArray("geometry-bytes"),
                "Main window geometry should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.mainWindowState() == QByteArray("state-bytes"),
                "Main window state should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.openDocumentPaths()
                    == QStringList({QStringLiteral("/tmp/project/a.th"), QStringLiteral("/tmp/project/b.th2")}),
                "Open document paths should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.activeDocumentPath() == QStringLiteral("/tmp/project/b.th2"),
                "Active document path should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.structureNameOverrides() == QStringLiteral("{\"a\":\"A\"}"),
                "Structure name overrides should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.therionExecutablePath() == QStringLiteral("/usr/bin/therion"),
                "Therion executable path should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.therionWorkingDirectory() == QStringLiteral("/tmp/project"),
                "Therion working directory should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.therionArguments() == QStringLiteral("-q thconfig"),
                "Therion arguments should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.therionMapTouchFriendlyControlsEnabled(),
                "Touch-friendly controls flag should round-trip through the injected settings file.")) {
        return 1;
    }
    if (!expect(store.therionMapBackgroundLayers() == QStringLiteral("[]"),
                "Map background layer metadata should round-trip through the injected settings file.")) {
        return 1;
    }

    return 0;
}

int runDefaultsTest()
{
    QTemporaryDir temporarySettingsDir;
    if (!expect(temporarySettingsDir.isValid(), "Temporary settings directory creation failed.")) {
        return 1;
    }

    const SessionSettingsStore store(temporarySettingsDir.filePath(QStringLiteral("empty.ini")));
    if (!expect(store.lastProjectPath().isEmpty(), "Default last project path should be empty.")) {
        return 1;
    }
    if (!expect(store.openDocumentPaths().isEmpty(), "Default open document path list should be empty.")) {
        return 1;
    }
    if (!expect(!store.therionMapTouchFriendlyControlsEnabled(),
                "Default touch-friendly controls flag should be false.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (const int result = runInstanceBackedRoundTripTest(); result != 0) {
        return result;
    }

    return runDefaultsTest();
}
