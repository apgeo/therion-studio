#include "TextEditorTab.h"

#include "TextEditorAppearanceController.h"
#include "TextEditorCursorController.h"
#include "DocumentFileInspector.h"
#include "TextEditorStatusController.h"
#include "block_editor/BlockEditorDirectiveRules.h"
#include "block_editor/BlockEditorDocumentOutlineBuilder.h"

#include <QPalette>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace TherionStudio
{
namespace
{
QString parentScopeForEntry(const BlockEditorDocumentOutline &outline, const BlockEditorDocumentEntry &entry)
{
    if (entry.parentLine <= 0) {
        return QStringLiteral("none");
    }
    const auto parentIndex = outline.entryIndexByStartLine.constFind(entry.parentLine);
    if (parentIndex == outline.entryIndexByStartLine.constEnd()) {
        return QStringLiteral("none");
    }
    return BlockEditorDirectiveRules::normalizeDirective(outline.entries.at(*parentIndex).kind);
}
}

void TextEditorTab::goToLine(int lineNumber)
{
    if (cursorController_ != nullptr) {
        cursorController_->goToLine(lineNumber);
    }
}

void TextEditorTab::goToLineColumn(int lineNumber, int columnNumber)
{
    if (cursorController_ != nullptr) {
        cursorController_->goToLineColumn(lineNumber, columnNumber);
    }
}

QString TextEditorTab::displayName() const
{
    return statusController_ != nullptr ? statusController_->displayName() : tr("Untitled");
}

bool TextEditorTab::isDirty() const
{
    return dirty_;
}

int TextEditorTab::currentLineNumber() const
{
    return cursorController_ != nullptr ? cursorController_->currentLineNumber() : 1;
}

int TextEditorTab::currentColumnNumber() const
{
    return cursorController_ != nullptr ? cursorController_->currentColumnNumber() : 1;
}

int TextEditorTab::documentRevision() const
{
    return editor_->document()->revision();
}

QString TextEditorTab::text() const
{
    return editor_->toPlainText();
}

void TextEditorTab::insertTextAtCursor(const QString &contents)
{
    if (editor_ == nullptr || contents.isEmpty()) {
        return;
    }

    QTextCursor cursor = editor_->textCursor();
    cursor.beginEditBlock();
    cursor.insertText(contents);
    cursor.endEditBlock();
    editor_->setTextCursor(cursor);
    editor_->setFocus();
}

TextEditorTab::ImportInsertionPoint TextEditorTab::resolveImportInsertionPoint() const
{
    ImportInsertionPoint point;
    if (!blocksModeActive_ || blockDetailsSelectedLineNumber_ <= 0 || editor_ == nullptr) {
        return point;
    }

    const BlockEditorDocumentOutline outline =
        BlockEditorDocumentOutlineBuilder(blockEditorDocumentOutlineContext()).buildFromContents(editor_->toPlainText());
    const auto entryIndex = outline.entryIndexByStartLine.constFind(blockDetailsSelectedLineNumber_);
    if (entryIndex == outline.entryIndexByStartLine.constEnd()) {
        return point;
    }

    const BlockEditorDocumentEntry entry = outline.entries.at(*entryIndex);
    const QString kind = BlockEditorDirectiveRules::normalizeDirective(entry.kind);
    point.blockSelection = true;
    if (BlockEditorDirectiveRules::isContainerBlockDirective(kind)) {
        point.insertAfterLine = entry.startLine;
        point.scopeToken = kind;
    } else {
        point.insertAfterLine = entry.endLine;
        point.scopeToken = parentScopeForEntry(outline, entry);
    }
    return point;
}

QString TextEditorTab::importInsertionScopeToken() const
{
    return resolveImportInsertionPoint().scopeToken;
}

void TextEditorTab::insertTextAfterLine(int lineNumber, const QString &contents)
{
    if (editor_ == nullptr || contents.isEmpty()) {
        return;
    }

    QTextCursor cursor(editor_->document());
    const int blockCount = editor_->document()->blockCount();
    if (lineNumber > 0 && lineNumber < blockCount) {
        const QTextBlock targetBlock = editor_->document()->findBlockByNumber(lineNumber);
        cursor.setPosition(targetBlock.isValid() ? targetBlock.position() : editor_->document()->characterCount() - 1);
    } else {
        cursor.movePosition(QTextCursor::End);
    }

    QString insertionText = contents;
    if (cursor.position() >= editor_->document()->characterCount() - 1
        && !editor_->toPlainText().isEmpty()
        && !editor_->toPlainText().endsWith(QLatin1Char('\n'))) {
        insertionText.prepend(QLatin1Char('\n'));
    }

    cursor.beginEditBlock();
    cursor.insertText(insertionText);
    cursor.endEditBlock();
    editor_->setTextCursor(cursor);
    editor_->setFocus();
}

void TextEditorTab::insertTextAtImportInsertionPoint(const QString &contents)
{
    const ImportInsertionPoint point = resolveImportInsertionPoint();
    if (!point.blockSelection || point.insertAfterLine <= 0) {
        insertTextAtCursor(contents);
        return;
    }
    insertTextAfterLine(point.insertAfterLine, contents);
}

QColor TextEditorTab::sourceSurfaceColor() const
{
    if (appearanceController_ == nullptr) {
        return palette().color(QPalette::Window);
    }
    return appearanceController_->sourceSurfaceColor();
}

QString TextEditorTab::statusPathText() const
{
    return statusController_ != nullptr ? statusController_->statusPathText() : tr("No file open");
}

QString TextEditorTab::statusEncodingText() const
{
    return statusController_ != nullptr ? statusController_->statusEncodingText() : QStringLiteral("UTF-8");
}

QString TextEditorTab::fileEncodingName() const
{
    return fileEncodingName_;
}

QString TextEditorTab::fileEncodingLabel() const
{
    return fileEncodingLabel_;
}

void TextEditorTab::triggerConvertToUtf8()
{
    handleConvertToUtf8Triggered();
}

bool TextEditorTab::canUndo() const
{
    return editor_ != nullptr && editor_->document() != nullptr && editor_->document()->isUndoAvailable();
}

bool TextEditorTab::canRedo() const
{
    return editor_ != nullptr && editor_->document() != nullptr && editor_->document()->isRedoAvailable();
}

void TextEditorTab::triggerUndo()
{
    if (editor_ != nullptr) {
        editor_->undo();
    }
}

void TextEditorTab::triggerRedo()
{
    if (editor_ != nullptr) {
        editor_->redo();
    }
}

void TextEditorTab::setInlineStatusVisible(bool visible)
{
    if (statusController_ != nullptr) {
        statusController_->setInlineStatusVisible(visible);
    }
}

bool TextEditorTab::isCurrentStateDirty() const
{
    return statusController_ != nullptr && statusController_->isCurrentStateDirty();
}

void TextEditorTab::applyDirtyStateFromCurrentState()
{
    if (statusController_ != nullptr) {
        statusController_->applyDirtyStateFromCurrentState();
    }
}

void TextEditorTab::handleCursorPositionChanged()
{
    if (cursorController_ != nullptr) {
        cursorController_->handleCursorPositionChanged();
    }
}

void TextEditorTab::refreshCurrentLineHighlight()
{
    if (cursorController_ != nullptr) {
        cursorController_->refreshCurrentLineHighlight();
    }
}

void TextEditorTab::refreshTitle()
{
    if (statusController_ != nullptr) {
        statusController_->refreshTitle();
    }
}

void TextEditorTab::refreshStatus()
{
    if (statusController_ != nullptr) {
        statusController_->refreshStatus();
    }
    if (rawFileInspector_ != nullptr) {
        rawFileInspector_->refresh();
    }
    if (blockFileInspector_ != nullptr) {
        blockFileInspector_->refresh();
    }
}
}
