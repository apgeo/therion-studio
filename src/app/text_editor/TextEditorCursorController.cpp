#include "TextEditorCursorController.h"

#include "raw_editor/RawEditorTextEdit.h"

#include <QFrame>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QWidget>
#include <QtGlobal>

#include <utility>

namespace TherionStudio
{
TextEditorCursorController::TextEditorCursorController(TextEditorCursorContext context)
    : context_(std::move(context))
{
}

void TextEditorCursorController::goToLine(int lineNumber)
{
    if (context_.editor == nullptr) {
        return;
    }

    if (context_.blocksModeActive != nullptr
        && *context_.blocksModeActive
        && context_.setBlocksModeActive) {
        context_.setBlocksModeActive(false);
    }

    if (lineNumber <= 0) {
        return;
    }

    const QTextBlock block = context_.editor->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    context_.editor->setTextCursor(cursor);
    setHighlightedLineNumber(lineNumber);
    context_.editor->centerCursor();
    context_.editor->ensureCursorVisible();
    refreshCurrentLineHighlight();
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }

    if (context_.searchBar != nullptr && context_.searchBar->isVisible() && context_.findEdit != nullptr) {
        context_.findEdit->clearFocus();
    }
}

void TextEditorCursorController::goToLineColumn(int lineNumber, int columnNumber)
{
    if (context_.editor == nullptr) {
        return;
    }

    if (context_.blocksModeActive != nullptr
        && *context_.blocksModeActive
        && context_.setBlocksModeActive) {
        context_.setBlocksModeActive(false);
    }

    if (lineNumber <= 0) {
        return;
    }

    const QTextBlock block = context_.editor->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    const int clampedColumn = qMax(1, columnNumber);
    QTextCursor cursor(block);
    const int offsetInBlock = qMin(clampedColumn - 1, qMax(0, block.length() - 1));
    cursor.setPosition(block.position() + offsetInBlock);
    context_.editor->setTextCursor(cursor);
    setHighlightedLineNumber(lineNumber);
    context_.editor->centerCursor();
    context_.editor->ensureCursorVisible();
    refreshCurrentLineHighlight();
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }

    if (context_.searchBar != nullptr && context_.searchBar->isVisible() && context_.findEdit != nullptr) {
        context_.findEdit->clearFocus();
    }
}

int TextEditorCursorController::currentLineNumber() const
{
    return context_.editor != nullptr
        ? context_.editor->textCursor().blockNumber() + 1
        : 1;
}

int TextEditorCursorController::currentColumnNumber() const
{
    return context_.editor != nullptr
        ? context_.editor->textCursor().positionInBlock() + 1
        : 1;
}

void TextEditorCursorController::handleCursorPositionChanged()
{
    if (context_.editor == nullptr
        || context_.loading == nullptr
        || *context_.loading
        || context_.currentLineNumber == nullptr
        || context_.currentColumnNumber == nullptr
        || context_.highlightedLineNumber == nullptr) {
        return;
    }

    const QTextCursor cursor = context_.editor->textCursor();
    const int currentLineNumber = cursor.blockNumber() + 1;
    const int currentColumnNumber = cursor.positionInBlock() + 1;
    const bool lineChanged = currentLineNumber != *context_.currentLineNumber;
    const bool columnChanged = currentColumnNumber != *context_.currentColumnNumber;
    if (currentLineNumber != *context_.currentLineNumber) {
        *context_.currentLineNumber = currentLineNumber;
        if (context_.currentLineChanged) {
            context_.currentLineChanged(*context_.currentLineNumber);
        }
    }
    if (columnChanged) {
        *context_.currentColumnNumber = currentColumnNumber;
    }
    if (lineChanged || columnChanged) {
        if (context_.cursorPositionChanged) {
            context_.cursorPositionChanged(*context_.currentLineNumber, *context_.currentColumnNumber);
        }
    }

    *context_.highlightedLineNumber = currentLineNumber;
    refreshCurrentLineHighlight();
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
    if (context_.updateValidationTooltipForCursor) {
        context_.updateValidationTooltipForCursor();
    }
}

void TextEditorCursorController::refreshCurrentLineHighlight()
{
    if (context_.editor == nullptr || context_.highlightedLineNumber == nullptr || context_.editor->isReadOnly()) {
        return;
    }

    if (auto *highlightEditor = dynamic_cast<RawEditorTextEdit *>(context_.editor)) {
        highlightEditor->setHighlightedLineNumber(*context_.highlightedLineNumber);
    }
}

void TextEditorCursorController::setHighlightedLineNumber(int lineNumber) const
{
    if (context_.highlightedLineNumber != nullptr) {
        *context_.highlightedLineNumber = lineNumber;
    }
}
}
