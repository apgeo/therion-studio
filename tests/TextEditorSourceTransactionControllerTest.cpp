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

TextEditorSourceTransactionRequest request(const QString &label, const QString &beforeText, const QString &afterText)
{
    return {
        .label = label,
        .beforeText = beforeText,
        .afterText = afterText,
        .undoStatusMessage = QStringLiteral("undo status"),
        .redoStatusMessage = QStringLiteral("redo status"),
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
    tab.replaceTextForCommand(afterText);
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
    if (runNoOpChangeDoesNotPushSnapshotTest() != 0) {
        return 1;
    }

    return 0;
}
