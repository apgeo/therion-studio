#include "../src/app/text_editor/TextEditorSourceTransactionController.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>

#include <iostream>
#include <utility>

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

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QString createTestFile(QTemporaryDir &tempDir, const QByteArray &contents)
{
    const QString filePath = tempDir.path() + QStringLiteral("/transaction-test.th");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(contents);
    file.close();
    return filePath;
}

bool loadTestTab(TextEditorTab *tab, const QString &filePath)
{
    QString errorMessage;
    if (!tab->loadFile(filePath, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << '\n';
        return false;
    }
    tab->resize(640, 480);
    tab->show();
    pumpEvents();
    return true;
}

TextEditorSourceTransactionRequest request(const QString &label,
                                           const QString &beforeText,
                                           const QString &afterText,
                                           int expectedSourceRevision = 0,
                                           const QString &staleStatusMessage = QString(),
                                           TextEditorSourceProjectionInvalidationPolicy projectionPolicy =
                                               TextEditorSourceProjectionInvalidationPolicy::FlushPendingRefresh,
                                           TextEditorSourceSelectionRestorePolicy selectionPolicy =
                                               TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection,
                                           std::function<void()> projectionHook = {},
                                           std::function<void()> selectionHook = {})
{
    return {
        .label = label,
        .beforeText = beforeText,
        .afterText = afterText,
        .expectedSourceRevision = expectedSourceRevision,
        .projectionInvalidationPolicy = projectionPolicy,
        .selectionRestorePolicy = selectionPolicy,
        .projectionInvalidationHook = std::move(projectionHook),
        .selectionRestoreHook = std::move(selectionHook),
        .undoStatusMessage = QStringLiteral("undo status"),
        .redoStatusMessage = QStringLiteral("redo status"),
        .staleStatusMessage = staleStatusMessage,
    };
}

TextEditorSourceTransactionRequest sourceEditRequest(const QString &label,
                                                     const QString &beforeText,
                                                     QVector<TherionSourceTextEdit> sourceEdits,
                                                     int expectedSourceRevision = 0,
                                                     const QString &staleStatusMessage = QString(),
                                                     TextEditorSourceProjectionInvalidationPolicy projectionPolicy =
                                                         TextEditorSourceProjectionInvalidationPolicy::FlushPendingRefresh,
                                                     TextEditorSourceSelectionRestorePolicy selectionPolicy =
                                                         TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection,
                                                     std::function<void()> projectionHook = {},
                                                     std::function<void()> selectionHook = {})
{
    return {
        .label = label,
        .beforeText = beforeText,
        .sourceEdits = std::move(sourceEdits),
        .expectedSourceRevision = expectedSourceRevision,
        .projectionInvalidationPolicy = projectionPolicy,
        .selectionRestorePolicy = selectionPolicy,
        .projectionInvalidationHook = std::move(projectionHook),
        .selectionRestoreHook = std::move(selectionHook),
        .undoStatusMessage = QStringLiteral("undo status"),
        .redoStatusMessage = QStringLiteral("redo status"),
        .staleStatusMessage = staleStatusMessage,
    };
}

int runApplyChangeCreatesUndoSnapshotTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "encoding utf-8\n");
    if (!expect(!filePath.isEmpty(), "Failed to create source transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load source transaction test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    QStringList statuses;

    TextEditorSourceTransactionController controller({
        .textEditor = &tab,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [&statuses](const QString &statusMessage) { statuses.append(statusMessage); },
    });

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("survey demo\nendsurvey\n");
    controller.applyChangeWithSnapshot(request(QStringLiteral("Append Survey"), beforeText, afterText));
    pumpEvents();

    if (!expect(tab.text() == afterText, "applyChangeWithSnapshot should apply afterText immediately.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "applyChangeWithSnapshot should push one undo snapshot.")) {
        return 1;
    }
    if (!expect(refreshCount == 1, "applyChangeWithSnapshot should flush pending refresh once.")) {
        return 1;
    }
    if (!expect(!commandApplyInProgress, "applyChangeWithSnapshot should restore commandApplyInProgress.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore beforeText.")) {
        return 1;
    }
    if (!expect(statuses.value(0) == QStringLiteral("undo status"), "Undo should emit undo status.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "Redo should restore afterText.")) {
        return 1;
    }
    if (!expect(statuses.value(1) == QStringLiteral("redo status"), "Redo should emit redo status.")) {
        return 1;
    }

    return 0;
}

int runRecordSnapshotForAlreadyAppliedChangeTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "encoding utf-8\n");
    if (!expect(!filePath.isEmpty(), "Failed to create source transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load source transaction test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    QStringList statuses;

    TextEditorSourceTransactionController controller({
        .textEditor = &tab,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [&statuses](const QString &statusMessage) { statuses.append(statusMessage); },
    });

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("# inserted by command\n");
    tab.applySourceSnapshotForTransaction(afterText);
    pumpEvents();

    controller.recordSnapshot(request(QStringLiteral("Already Applied"), beforeText, afterText));
    if (!expect(undoStack.count() == 1, "recordSnapshot should push one undo snapshot.")) {
        return 1;
    }
    if (!expect(refreshCount == 1, "recordSnapshot should flush pending refresh once.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "recordSnapshot undo should restore beforeText.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "recordSnapshot redo should restore afterText.")) {
        return 1;
    }
    if (!expect(statuses == QStringList({QStringLiteral("undo status"), QStringLiteral("redo status")}),
                "recordSnapshot should emit undo and redo status messages.")) {
        return 1;
    }

    return 0;
}

int runApplySourceEditsCreatesUndoSnapshotTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "encoding utf-8\nsurvey old\nendsurvey\n");
    if (!expect(!filePath.isEmpty(), "Failed to create source edit transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load source edit transaction test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    QStringList statuses;

    TextEditorSourceTransactionController controller({
        .textEditor = &tab,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [&statuses](const QString &statusMessage) { statuses.append(statusMessage); },
    });

    const QString beforeText = tab.text();
    const int editStart = beforeText.indexOf(QStringLiteral("old"));
    if (!expect(editStart >= 0, "Source edit transaction test should find the target token.")) {
        return 1;
    }
    const QString afterText = QStringLiteral("encoding utf-8\nsurvey new\nendsurvey\n");
    controller.applyChangeWithSnapshot(sourceEditRequest(QStringLiteral("Rename Survey"),
                                                         beforeText,
                                                         {TherionSourceTextEdit{editStart, 3, QStringLiteral("new")}}));
    pumpEvents();

    if (!expect(tab.text() == afterText, "Source edit transaction should apply source edits immediately.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Source edit transaction should push one undo snapshot.")) {
        return 1;
    }
    if (!expect(refreshCount == 1, "Source edit transaction should flush pending refresh once.")) {
        return 1;
    }
    if (!expect(!commandApplyInProgress, "Source edit transaction should restore commandApplyInProgress.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Source edit transaction undo should restore beforeText.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "Source edit transaction redo should restore the computed afterText.")) {
        return 1;
    }
    if (!expect(statuses == QStringList({QStringLiteral("undo status"), QStringLiteral("redo status")}),
                "Source edit transaction should emit undo and redo status messages.")) {
        return 1;
    }

    return 0;
}

int runNoOpChangeDoesNotPushSnapshotTest()
{
    QUndoStack undoStack;
    int refreshCount = 0;
    bool commandApplyInProgress = false;
    TextEditorSourceTransactionController controller({
        .textEditor = nullptr,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [](const QString &) {},
    });

    const QString text = QStringLiteral("encoding utf-8\n");
    controller.applyChangeWithSnapshot(request(QStringLiteral("No-op"), text, text));
    controller.recordSnapshot(request(QStringLiteral("No-op"), text, text));

    if (!expect(undoStack.count() == 0, "No-op source transaction should not push undo snapshots.")) {
        return 1;
    }
    if (!expect(refreshCount == 0, "No-op source transaction should not flush refresh callbacks.")) {
        return 1;
    }

    return 0;
}

int runApplyChangeWithStaleBeforeTextRejectedTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "encoding utf-8\n");
    if (!expect(!filePath.isEmpty(), "Failed to create source transaction stale-before test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load source transaction stale-before test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    QStringList statuses;

    TextEditorSourceTransactionController controller({
        .textEditor = &tab,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [&statuses](const QString &statusMessage) { statuses.append(statusMessage); },
    });

    const QString beforeText = tab.text();
    const int beforeRevision = tab.documentRevision();
    const QString afterText = beforeText + QStringLiteral("survey stale\nendsurvey\n");

    const QString driftText = beforeText + QStringLiteral("# concurrent edit\n");
    tab.applySourceSnapshotForTransaction(driftText);
    pumpEvents();

    controller.applyChangeWithSnapshot(request(QStringLiteral("Stale Apply"),
                                              beforeText,
                                              afterText,
                                              beforeRevision,
                                              QStringLiteral("stale status")));
    pumpEvents();

    if (!expect(tab.text() == driftText, "Stale applyChangeWithSnapshot should not overwrite concurrent source changes.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 0, "Stale applyChangeWithSnapshot should not push undo snapshots.")) {
        return 1;
    }
    if (!expect(refreshCount == 0, "Stale applyChangeWithSnapshot should not flush refresh callbacks.")) {
        return 1;
    }
    if (!expect(statuses == QStringList({QStringLiteral("stale status")}),
                "Stale applyChangeWithSnapshot should emit stale status.")) {
        return 1;
    }

    return 0;
}

int runRecordSnapshotWithStaleAfterTextRejectedTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "encoding utf-8\n");
    if (!expect(!filePath.isEmpty(), "Failed to create source transaction stale-record test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load source transaction stale-record test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    QStringList statuses;

    TextEditorSourceTransactionController controller({
        .textEditor = &tab,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [&statuses](const QString &statusMessage) { statuses.append(statusMessage); },
    });

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("# applied change\n");
    tab.applySourceSnapshotForTransaction(afterText);
    pumpEvents();
    const int appliedRevision = tab.documentRevision();

    const QString driftText = afterText + QStringLiteral("# concurrent edit\n");
    tab.applySourceSnapshotForTransaction(driftText);
    pumpEvents();

    controller.recordSnapshot(request(QStringLiteral("Stale Record"),
                                     beforeText,
                                     afterText,
                                     appliedRevision,
                                     QStringLiteral("stale status")));
    pumpEvents();

    if (!expect(tab.text() == driftText, "Stale recordSnapshot should preserve concurrent source changes.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 0, "Stale recordSnapshot should not push undo snapshots.")) {
        return 1;
    }
    if (!expect(refreshCount == 0, "Stale recordSnapshot should not flush refresh callbacks.")) {
        return 1;
    }
    if (!expect(statuses == QStringList({QStringLiteral("stale status")}),
                "Stale recordSnapshot should emit stale status.")) {
        return 1;
    }

    return 0;
}

int runCustomPolicyHooksTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "encoding utf-8\n");
    if (!expect(!filePath.isEmpty(), "Failed to create source transaction custom-policy test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load source transaction custom-policy test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int projectionHookCount = 0;
    int selectionHookCount = 0;

    TextEditorSourceTransactionController controller({
        .textEditor = &tab,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [](const QString &) {},
    });

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("survey custom\nendsurvey\n");
    controller.applyChangeWithSnapshot(request(QStringLiteral("Custom Policy"),
                                              beforeText,
                                              afterText,
                                              0,
                                              QString(),
                                              TextEditorSourceProjectionInvalidationPolicy::CustomHook,
                                              TextEditorSourceSelectionRestorePolicy::CustomHook,
                                              [&projectionHookCount]() { ++projectionHookCount; },
                                              [&selectionHookCount]() { ++selectionHookCount; }));
    pumpEvents();

    if (!expect(tab.text() == afterText, "Custom policy applyChangeWithSnapshot should still apply afterText.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Custom policy applyChangeWithSnapshot should still push an undo snapshot.")) {
        return 1;
    }
    if (!expect(refreshCount == 0, "Custom projection policy should skip context flush callback.")) {
        return 1;
    }
    if (!expect(projectionHookCount == 1, "Custom projection policy should execute projection hook once.")) {
        return 1;
    }
    if (!expect(selectionHookCount == 1, "Custom selection policy should execute selection hook once.")) {
        return 1;
    }

    return 0;
}

int runProjectionNonePolicySkipsRefreshTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "encoding utf-8\n");
    if (!expect(!filePath.isEmpty(), "Failed to create source transaction no-projection test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load source transaction no-projection test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    bool commandApplyInProgress = false;
    int refreshCount = 0;

    TextEditorSourceTransactionController controller({
        .textEditor = &tab,
        .undoStack = &undoStack,
        .commandApplyInProgress = &commandApplyInProgress,
        .flushPendingRefresh = [&refreshCount]() { ++refreshCount; },
        .statusCallback = [](const QString &) {},
    });

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("survey none\nendsurvey\n");
    controller.applyChangeWithSnapshot(request(QStringLiteral("No Projection Flush"),
                                              beforeText,
                                              afterText,
                                              0,
                                              QString(),
                                              TextEditorSourceProjectionInvalidationPolicy::None));
    pumpEvents();

    if (!expect(tab.text() == afterText, "Projection-none applyChangeWithSnapshot should still apply afterText.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Projection-none applyChangeWithSnapshot should still push an undo snapshot.")) {
        return 1;
    }
    if (!expect(refreshCount == 0, "Projection-none policy should suppress context flush callback.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    if (runApplyChangeCreatesUndoSnapshotTest() != 0) {
        return 1;
    }
    if (runRecordSnapshotForAlreadyAppliedChangeTest() != 0) {
        return 1;
    }
    if (runApplySourceEditsCreatesUndoSnapshotTest() != 0) {
        return 1;
    }
    if (runNoOpChangeDoesNotPushSnapshotTest() != 0) {
        return 1;
    }
    if (runApplyChangeWithStaleBeforeTextRejectedTest() != 0) {
        return 1;
    }
    if (runRecordSnapshotWithStaleAfterTextRejectedTest() != 0) {
        return 1;
    }
    if (runCustomPolicyHooksTest() != 0) {
        return 1;
    }
    if (runProjectionNonePolicySkipsRefreshTest() != 0) {
        return 1;
    }

    return 0;
}
