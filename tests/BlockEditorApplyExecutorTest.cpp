#include "../src/app/text_editor/block_editor/BlockEditorApplyExecutor.h"

#include <QApplication>
#include <QPlainTextEdit>
#include <QStringList>
#include <QTextCursor>

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

void replaceTextWithUndo(QPlainTextEdit *editor, const QString &contents)
{
    QTextCursor rewriteCursor(editor->document());
    rewriteCursor.beginEditBlock();
    rewriteCursor.select(QTextCursor::Document);
    rewriteCursor.insertText(contents);
    rewriteCursor.endEditBlock();
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
        sourceContext.replaceText = [&editor](const QString &contents) {
            replaceTextWithUndo(&editor, contents);
        };
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

    if (runAutoCommitCreatesUndoableTextChangeTest() != 0) {
        return 1;
    }

    return 0;
}
