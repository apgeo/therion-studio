#include "../src/app/MainWindowSessionController.h"
#include "FakeSessionStore.h"

#include <QCoreApplication>
#include <QTemporaryDir>
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

int runRestoredProjectWorkflowTest()
{
    FakeSessionStore sessionStore;
    sessionStore.setMainWindowGeometry(QByteArrayLiteral("geometry"));
    sessionStore.setMainWindowState(QByteArrayLiteral("state"));

    QTemporaryDir projectDir;
    if (!expect(projectDir.isValid(),
                "Temporary directory setup failed for session restore test.")) {
        return 1;
    }
    sessionStore.setLastProjectPath(projectDir.path());

    QStringList calls;
    MainWindowSessionController::RestoreSessionActions actions;
    actions.restoreGeometry = [&calls](const QByteArray &geometry) {
        calls.append(QStringLiteral("restore_geometry:%1").arg(QString::fromUtf8(geometry)));
        return true;
    };
    actions.restoreState = [&calls](const QByteArray &state) {
        calls.append(QStringLiteral("restore_state:%1").arg(QString::fromUtf8(state)));
    };
    actions.resizeToDefault = [&calls]() { calls.append(QStringLiteral("resize_default")); };
    actions.ensureUsableWindowSize = [&calls]() { calls.append(QStringLiteral("ensure_usable")); };
    actions.applyProjectRootToBrowser = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("apply_root:%1").arg(projectPath));
    };
    actions.appendConsoleLine = [&calls](const QString &line) {
        calls.append(QStringLiteral("console:%1").arg(line));
    };
    actions.loadStructureNameOverrides = [&calls]() { calls.append(QStringLiteral("load_overrides")); };
    actions.syncOpenDocumentsToProjectRoot = [&calls]() { calls.append(QStringLiteral("sync_docs")); };
    actions.rebuildStructureSidebar = [&calls]() { calls.append(QStringLiteral("rebuild_structure")); };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };
    actions.restoreOpenDocuments = [&calls]() { calls.append(QStringLiteral("restore_docs")); };

    MainWindowSessionController::restoreSession(sessionStore, actions);

    const QStringList expected = {
        QStringLiteral("restore_geometry:geometry"),
        QStringLiteral("restore_state:state"),
        QStringLiteral("ensure_usable"),
        QStringLiteral("apply_root:%1").arg(projectDir.path()),
        QStringLiteral("console:Restored project root %1").arg(projectDir.path()),
        QStringLiteral("load_overrides"),
        QStringLiteral("sync_docs"),
        QStringLiteral("rebuild_structure"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions"),
        QStringLiteral("restore_docs")};
    if (!expect(calls == expected,
                "Session controller restored-project workflow order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runGeometryFailureFallbackTest()
{
    FakeSessionStore sessionStore;
    sessionStore.setMainWindowGeometry(QByteArrayLiteral("geometry"));
    sessionStore.setMainWindowState(QByteArray());
    sessionStore.setLastProjectPath(QString());

    QStringList calls;
    MainWindowSessionController::RestoreSessionActions actions;
    actions.restoreGeometry = [&calls](const QByteArray &) {
        calls.append(QStringLiteral("restore_geometry"));
        return false;
    };
    actions.restoreState = [&calls](const QByteArray &) { calls.append(QStringLiteral("restore_state")); };
    actions.resizeToDefault = [&calls]() { calls.append(QStringLiteral("resize_default")); };
    actions.ensureUsableWindowSize = [&calls]() { calls.append(QStringLiteral("ensure_usable")); };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };
    actions.restoreOpenDocuments = [&calls]() { calls.append(QStringLiteral("restore_docs")); };

    MainWindowSessionController::restoreSession(sessionStore, actions);

    const QStringList expected = {
        QStringLiteral("restore_geometry"),
        QStringLiteral("resize_default"),
        QStringLiteral("ensure_usable"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions"),
        QStringLiteral("restore_docs")};
    if (!expect(calls == expected,
                "Session controller geometry-fallback workflow order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runRestoredProjectWorkflowTest() != 0) {
        return 1;
    }
    if (runGeometryFailureFallbackTest() != 0) {
        return 1;
    }

    return 0;
}
