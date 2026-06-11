#include "../src/app/text_editor/block_editor/BlockEditorApplyExecutor.h"
#include "../src/app/text_editor/block_editor/BlockEditorSourceController.h"
#include "../src/app/text_editor/block_editor/BlockEditorSourceText.h"
#include "../src/core/TherionDocumentEditor.h"

#include <QApplication>
#include <QPlainTextEdit>
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

int runSourceLineRangeReplacementEditTest()
{
    const QString contents = QStringLiteral(
        "survey cave\r\n"
        "scrap s1 -projection plan\n"
        "endscrap\r"
        "endsurvey\n");
    TherionSourceTextEdit edit;
    if (!expect(blockEditorSourceLineRangeReplacementEdit(contents,
                                                          2,
                                                          2,
                                                          QStringList{QStringLiteral("scrap s1 -projection none")},
                                                          &edit),
                "Block source range planner should build a replacement edit.")) {
        return 1;
    }

    const int expectedOffset = contents.indexOf(QStringLiteral("plan"));
    if (!expect(edit.startOffset == expectedOffset
                    && edit.length == QStringLiteral("plan").size()
                    && edit.replacementText == QStringLiteral("none"),
                "Block source range planner should expose the minimal changed source range.")) {
        return 1;
    }

    QString updatedContents = contents;
    updatedContents.replace(edit.startOffset, edit.length, edit.replacementText);
    if (!expect(updatedContents == QStringLiteral("survey cave\r\n"
                                                  "scrap s1 -projection none\n"
                                                  "endscrap\r"
                                                  "endsurvey\n"),
                "Block source range planner should preserve surrounding physical line endings.")) {
        return 1;
    }

    return 0;
}

int runSourceControllerLineEditTest()
{
    QPlainTextEdit editor;
    editor.setPlainText(QStringLiteral("survey cave\n"
                                       "scrap existing\n"
                                       "endsurvey\n"));
    editor.document()->clearUndoRedoStacks();

    BlockEditorSourceContext sourceContext;
    sourceContext.editor = &editor;
    sourceContext.editable = true;
    const BlockEditorSourceController source(sourceContext);

    QString errorMessage;
    if (!expect(source.insertLinesBefore(2, QStringList{QStringLiteral("scrap inserted")}, &errorMessage),
                "Block source controller should insert lines through a source edit.")) {
        return 1;
    }
    if (!expect(editor.toPlainText() == QStringLiteral("survey cave\n"
                                                       "scrap inserted\n"
                                                       "scrap existing\n"
                                                       "endsurvey\n"),
                "Block source controller insertion should update the source text.")) {
        return 1;
    }

    if (!expect(source.replaceLine(2, QStringLiteral("scrap changed")),
                "Block source controller should replace a line through a source edit.")) {
        return 1;
    }
    if (!expect(editor.toPlainText() == QStringLiteral("survey cave\n"
                                                       "scrap changed\n"
                                                       "scrap existing\n"
                                                       "endsurvey\n"),
                "Block source controller replacement should update the selected line.")) {
        return 1;
    }

    if (!expect(source.removeLineRange(2, 2),
                "Block source controller should remove lines through a source edit.")) {
        return 1;
    }
    if (!expect(editor.toPlainText() == QStringLiteral("survey cave\n"
                                                       "scrap existing\n"
                                                       "endsurvey\n"),
                "Block source controller removal should delete the selected line range.")) {
        return 1;
    }
    if (!expect(editor.document()->isUndoAvailable(),
                "Block source controller line edits should remain undoable.")) {
        return 1;
    }

    editor.undo();
    if (!expect(editor.toPlainText() == QStringLiteral("survey cave\n"
                                                       "scrap changed\n"
                                                       "scrap existing\n"
                                                       "endsurvey\n"),
                "Undo after source controller removal should restore the removed line.")) {
        return 1;
    }

    return 0;
}

int runAutoCommitCreatesUndoableTextChangeTest()
{
    QPlainTextEdit editor;
    const QString beforeText = QStringLiteral(
        "survey cave\n"
        "scrap s1 -projection plan\n"
        "endscrap\n"
        "endsurvey\n");
    const QString expectedAfterText = QStringLiteral(
        "survey cave\n"
        "scrap s1 -projection none\n"
        "endscrap\n"
        "endsurvey\n");
    editor.setPlainText(beforeText);
    editor.document()->clearUndoRedoStacks();

    bool tearingDown = false;
    bool detailsPopulating = false;
    int selectedLineNumber = 2;
    int selectedLineAfterApply = 0;
    int refreshCount = 0;

    BlockEditorApplyExecutorContext context;
    context.tearingDown = &tearingDown;
    context.detailsPopulating = &detailsPopulating;
    context.selectedLineNumber = &selectedLineNumber;
    context.sourceContext = [&editor]() {
        BlockEditorSourceContext sourceContext;
        sourceContext.editor = &editor;
        sourceContext.editable = true;
        return sourceContext;
    };
    context.buildUpdatedLine = [](QString *updatedLine, QString *validationError) {
        if (validationError != nullptr) {
            validationError->clear();
        }
        if (updatedLine != nullptr) {
            *updatedLine = QStringLiteral("scrap s1 -projection none");
        }
        return true;
    };
    context.selectBlockInCanvasAndDetails = [&selectedLineAfterApply](int lineNumber) {
        selectedLineAfterApply = lineNumber;
    };
    context.refreshApplyState = [&refreshCount]() {
        ++refreshCount;
    };

    BlockEditorApplyExecutor executor(context);
    executor.applyChanges();

    if (!expect(editor.toPlainText() == expectedAfterText,
                "Block auto-commit should replace the selected logical line.")) {
        return 1;
    }
    if (!expect(editor.document()->isUndoAvailable(),
                "Block auto-commit should create an undoable text edit.")) {
        return 1;
    }
    if (!expect(selectedLineAfterApply == selectedLineNumber,
                "Block auto-commit should restore selection on the edited line.")) {
        return 1;
    }
    if (!expect(refreshCount == 1,
                "Block auto-commit should refresh apply state once after a changed line.")) {
        return 1;
    }

    editor.undo();
    if (!expect(editor.toPlainText() == beforeText,
                "Undo after block auto-commit should restore the original source text.")) {
        return 1;
    }
    if (!expect(editor.document()->isRedoAvailable(),
                "Undo after block auto-commit should enable redo.")) {
        return 1;
    }

    editor.redo();
    if (!expect(editor.toPlainText() == expectedAfterText,
                "Redo after block auto-commit should restore the edited source text.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (runSourceLineRangeReplacementEditTest() != 0) {
        return 1;
    }

    if (runSourceControllerLineEditTest() != 0) {
        return 1;
    }

    if (runAutoCommitCreatesUndoableTextChangeTest() != 0) {
        return 1;
    }

    return 0;
}
