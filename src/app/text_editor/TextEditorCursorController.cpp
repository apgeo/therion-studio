#include "TextEditorCursorController.h"

#include "TextEditorTab.h"
#include "raw_editor/RawEditorTextEdit.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QWidget>
#include <QtGlobal>

namespace TherionStudio
{
TextEditorCursorController::TextEditorCursorController(TextEditorTab *owner)
    : owner_(owner)
{
}

void TextEditorCursorController::goToLine(int lineNumber)
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return;
    }

    if (owner_->blocksModeActive_) {
        owner_->setBlocksModeActive(false);
    }

    if (lineNumber <= 0) {
        return;
    }

    const QTextBlock block = owner_->editor_->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    QTextCursor cursor(block);
    cursor.movePosition(QTextCursor::StartOfBlock);
    owner_->editor_->setTextCursor(cursor);
    owner_->highlightedLineNumber_ = lineNumber;
    owner_->editor_->centerCursor();
    owner_->editor_->ensureCursorVisible();
    refreshCurrentLineHighlight();
    owner_->updateContextHelp();

    if (owner_->searchBar_ != nullptr && owner_->searchBar_->isVisible() && owner_->findEdit_ != nullptr) {
        owner_->findEdit_->clearFocus();
    }
}

void TextEditorCursorController::goToLineColumn(int lineNumber, int columnNumber)
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return;
    }

    if (owner_->blocksModeActive_) {
        owner_->setBlocksModeActive(false);
    }

    if (lineNumber <= 0) {
        return;
    }

    const QTextBlock block = owner_->editor_->document()->findBlockByLineNumber(lineNumber - 1);
    if (!block.isValid()) {
        return;
    }

    const int clampedColumn = qMax(1, columnNumber);
    QTextCursor cursor(block);
    const int offsetInBlock = qMin(clampedColumn - 1, qMax(0, block.length() - 1));
    cursor.setPosition(block.position() + offsetInBlock);
    owner_->editor_->setTextCursor(cursor);
    owner_->highlightedLineNumber_ = lineNumber;
    owner_->editor_->centerCursor();
    owner_->editor_->ensureCursorVisible();
    refreshCurrentLineHighlight();
    owner_->updateContextHelp();

    if (owner_->searchBar_ != nullptr && owner_->searchBar_->isVisible() && owner_->findEdit_ != nullptr) {
        owner_->findEdit_->clearFocus();
    }
}

int TextEditorCursorController::currentLineNumber() const
{
    return owner_ != nullptr && owner_->editor_ != nullptr
        ? owner_->editor_->textCursor().blockNumber() + 1
        : 1;
}

int TextEditorCursorController::currentColumnNumber() const
{
    return owner_ != nullptr && owner_->editor_ != nullptr
        ? owner_->editor_->textCursor().positionInBlock() + 1
        : 1;
}

void TextEditorCursorController::handleCursorPositionChanged()
{
    if (owner_ == nullptr || owner_->editor_ == nullptr || owner_->loading_) {
        return;
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    const int currentLineNumber = cursor.blockNumber() + 1;
    const int currentColumnNumber = cursor.positionInBlock() + 1;
    const bool lineChanged = currentLineNumber != owner_->currentLineNumber_;
    const bool columnChanged = currentColumnNumber != owner_->currentColumnNumber_;
    if (currentLineNumber != owner_->currentLineNumber_) {
        owner_->currentLineNumber_ = currentLineNumber;
        emit owner_->currentLineChanged(owner_->currentLineNumber_);
    }
    if (columnChanged) {
        owner_->currentColumnNumber_ = currentColumnNumber;
    }
    if (lineChanged || columnChanged) {
        emit owner_->cursorPositionChanged(owner_->currentLineNumber_, owner_->currentColumnNumber_);
    }

    owner_->highlightedLineNumber_ = currentLineNumber;
    refreshCurrentLineHighlight();
    owner_->updateContextHelp();
    owner_->updateValidationTooltipForCursor();
}

void TextEditorCursorController::refreshCurrentLineHighlight()
{
    if (owner_ == nullptr || owner_->editor_ == nullptr || owner_->editor_->isReadOnly()) {
        return;
    }

    if (auto *highlightEditor = dynamic_cast<RawEditorTextEdit *>(owner_->editor_)) {
        highlightEditor->setHighlightedLineNumber(owner_->highlightedLineNumber_);
    }
}
}
