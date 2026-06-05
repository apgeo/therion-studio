#include "../src/app/text_editor/map_editor/MapEditorCanvasEditController.h"
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
    const QString filePath = tempDir.path() + QStringLiteral("/map-source-transaction.th2");
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

MapEditorCanvasEditController makeController(TextEditorTab *tab,
                                             QUndoStack *undoStack,
                                             QString *toolbarStatus,
                                             bool *commandApplyInProgress,
                                             int *refreshCount,
                                             int *flushCount)
{
    MapEditorCanvasEditContext context;
    context.textEditor = tab;
    context.undoStack = undoStack;
    context.toolbarStatusNote = toolbarStatus;
    context.commandApplyInProgress = commandApplyInProgress;
    context.refreshToolbarSummary = [refreshCount]() {
        ++(*refreshCount);
    };
    context.flushPendingSceneRefreshAfterCommand = [flushCount]() {
        ++(*flushCount);
    };
    return MapEditorCanvasEditController(context);
}

int runApplySourceTextChangeWithSnapshotTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "scrap s1\nendscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map source transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map source transaction test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount);

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("point 1 2 station\n");
    controller.applySourceTextChangeWithSnapshot(QStringLiteral("Insert Map Point"), beforeText, afterText, 3);
    pumpEvents();

    if (!expect(tab.text() == afterText, "applySourceTextChangeWithSnapshot should apply afterText immediately.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "applySourceTextChangeWithSnapshot should push one undo snapshot.")) {
        return 1;
    }
    if (!expect(flushCount == 1, "applySourceTextChangeWithSnapshot should flush pending scene refresh once.")) {
        return 1;
    }
    if (!expect(!commandApplyInProgress, "applySourceTextChangeWithSnapshot should restore commandApplyInProgress.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore source text before map source change.")) {
        return 1;
    }
    if (!expect(toolbarStatus == QStringLiteral("Removed inserted map object at source line 3."),
                "Undo should surface the map source snapshot undo status.")) {
        return 1;
    }
    if (!expect(refreshCount == 1, "Undo status should refresh toolbar summary once.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "Redo should restore source text after map source change.")) {
        return 1;
    }
    if (!expect(toolbarStatus == QStringLiteral("Restored inserted map object at source line 3."),
                "Redo should surface the map source snapshot redo status.")) {
        return 1;
    }
    if (!expect(refreshCount == 2, "Redo status should refresh toolbar summary again.")) {
        return 1;
    }

    return 0;
}

int runRecordSourceTextSnapshotForAlreadyAppliedChangeTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString filePath = createTestFile(tempDir, "scrap s1\nendscrap\n");
    if (!expect(!filePath.isEmpty(), "Failed to create map source snapshot test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load map source snapshot test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString toolbarStatus;
    bool commandApplyInProgress = false;
    int refreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController controller =
        makeController(&tab, &undoStack, &toolbarStatus, &commandApplyInProgress, &refreshCount, &flushCount);

    const QString beforeText = tab.text();
    const QString afterText = beforeText + QStringLiteral("line wall\n  0 0\n  1 1\nendline\n");
    tab.replaceTextForCommand(afterText);
    pumpEvents();

    controller.recordSourceTextSnapshot(QStringLiteral("Insert Map Line"), beforeText, afterText, 3);
    if (!expect(undoStack.count() == 1, "recordSourceTextSnapshot should push one undo snapshot.")) {
        return 1;
    }
    if (!expect(flushCount == 1, "recordSourceTextSnapshot should flush pending scene refresh once.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "recordSourceTextSnapshot undo should restore beforeText.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == afterText, "recordSourceTextSnapshot redo should restore afterText.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (runApplySourceTextChangeWithSnapshotTest() != 0) {
        return 1;
    }
    if (runRecordSourceTextSnapshotForAlreadyAppliedChangeTest() != 0) {
        return 1;
    }

    return 0;
}
