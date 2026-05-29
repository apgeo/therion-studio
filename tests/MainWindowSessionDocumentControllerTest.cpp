#include "../src/app/MainWindowSessionDocumentController.h"
#include "FakeSessionStore.h"

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

int runRestoreOpenDocumentsWorkflowTest()
{
    FakeSessionStore sessionStore;
    sessionStore.setOpenDocumentPaths({
        QStringLiteral("/tmp/map.th2"),
        QStringLiteral("/tmp/unsupported.dat"),
        QStringLiteral("/tmp/source.th")});
    sessionStore.setActiveDocumentPath(QStringLiteral("/tmp/source.th"));

    QStringList calls;
    MainWindowSessionDocumentController::RestoreOpenDocumentsActions actions;
    actions.openMapEditorDocument = [&calls](const QString &path) {
        calls.append(QStringLiteral("open_map:%1").arg(path));
    };
    actions.openTextEditorDocument = [&calls](const QString &path) {
        calls.append(QStringLiteral("open_text:%1").arg(path));
        return path != QStringLiteral("/tmp/unsupported.dat");
    };
    actions.appendSkippedUnsupportedDocumentConsole = [&calls](const QString &path) {
        calls.append(QStringLiteral("unsupported:%1").arg(path));
    };
    actions.activateRestoredDocumentPath = [&calls](const QString &path) {
        calls.append(QStringLiteral("activate:%1").arg(path));
    };
    actions.persistOpenDocuments = [&calls]() {
        calls.append(QStringLiteral("persist_open_documents"));
    };

    MainWindowSessionDocumentController::restoreOpenDocuments(sessionStore, actions);

    const QStringList expected = {
        QStringLiteral("open_map:/tmp/map.th2"),
        QStringLiteral("open_text:/tmp/unsupported.dat"),
        QStringLiteral("unsupported:/tmp/unsupported.dat"),
        QStringLiteral("open_text:/tmp/source.th"),
        QStringLiteral("activate:/tmp/source.th"),
        QStringLiteral("persist_open_documents")};
    if (!expect(calls == expected,
                "Session document restore workflow order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runRestoreOpenDocumentsNoEntriesTest()
{
    FakeSessionStore sessionStore;
    sessionStore.setOpenDocumentPaths({});

    bool anyCall = false;
    MainWindowSessionDocumentController::RestoreOpenDocumentsActions actions;
    actions.openMapEditorDocument = [&anyCall](const QString &) { anyCall = true; };
    actions.openTextEditorDocument = [&anyCall](const QString &) {
        anyCall = true;
        return true;
    };
    actions.activateRestoredDocumentPath = [&anyCall](const QString &) { anyCall = true; };
    actions.persistOpenDocuments = [&anyCall]() { anyCall = true; };

    MainWindowSessionDocumentController::restoreOpenDocuments(sessionStore, actions);

    if (!expect(!anyCall,
                "Session document restore should be a no-op when no documents are stored.")) {
        return 1;
    }

    return 0;
}

int runPersistOpenDocumentsDetachedActivePrecedenceTest()
{
    FakeSessionStore sessionStore;
    MainWindowSessionDocumentController::PersistOpenDocumentsSnapshot snapshot;
    snapshot.tabDocumentPaths = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th")};
    snapshot.detachedMapDocumentPaths = {
        QStringLiteral("/tmp/b.th"),
        QStringLiteral("/tmp/c.th2")};
    snapshot.activeDetachedDocumentPaths = {
        QStringLiteral("/tmp/c.th2")};
    snapshot.currentDocumentPath = QStringLiteral("/tmp/b.th");

    MainWindowSessionDocumentController::persistOpenDocuments(snapshot, sessionStore);

    const QStringList expectedPaths = {
        QStringLiteral("/tmp/a.th"),
        QStringLiteral("/tmp/b.th"),
        QStringLiteral("/tmp/c.th2")};
    if (!expect(sessionStore.openDocumentPaths() == expectedPaths,
                "Persisted open-document list should merge tab and detached paths.")) {
        return 1;
    }
    if (!expect(sessionStore.activeDocumentPath() == QStringLiteral("/tmp/c.th2"),
                "Active detached map should take precedence for persisted active path.")) {
        return 1;
    }

    return 0;
}

int runPersistOpenDocumentsCurrentFallbackTest()
{
    FakeSessionStore sessionStore;
    MainWindowSessionDocumentController::PersistOpenDocumentsSnapshot snapshot;
    snapshot.tabDocumentPaths = {
        QStringLiteral("/tmp/a.th")};
    snapshot.detachedMapDocumentPaths = {};
    snapshot.activeDetachedDocumentPaths = {};
    snapshot.currentDocumentPath = QStringLiteral("/tmp/a.th");

    MainWindowSessionDocumentController::persistOpenDocuments(snapshot, sessionStore);

    if (!expect(sessionStore.activeDocumentPath() == QStringLiteral("/tmp/a.th"),
                "Persisted active path should fall back to current tab document.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runRestoreOpenDocumentsWorkflowTest() != 0) {
        return 1;
    }
    if (runRestoreOpenDocumentsNoEntriesTest() != 0) {
        return 1;
    }
    if (runPersistOpenDocumentsDetachedActivePrecedenceTest() != 0) {
        return 1;
    }
    if (runPersistOpenDocumentsCurrentFallbackTest() != 0) {
        return 1;
    }

    return 0;
}
