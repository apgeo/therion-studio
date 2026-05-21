#include "BlockEditorSourceController.h"

#include "BlockEditorSourceText.h"
#include "../TextEditorTab.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>

namespace TherionStudio
{
BlockEditorSourceController::BlockEditorSourceController(TextEditorTab *owner)
    : owner_(owner)
    , mutableOwner_(owner)
{
}

BlockEditorSourceController::BlockEditorSourceController(const TextEditorTab *owner)
    : owner_(owner)
{
}

bool BlockEditorSourceController::hasEditor() const
{
    return owner_ != nullptr && owner_->editor_ != nullptr;
}

QString BlockEditorSourceController::text() const
{
    return hasEditor() ? owner_->editor_->toPlainText() : QString();
}

QStringList BlockEditorSourceController::normalizedLines() const
{
    return blockEditorNormalizedSourceLines(text());
}

bool BlockEditorSourceController::insertLinesBefore(int lineNumber,
                                                    const QStringList &newLines,
                                                    QString *errorMessage) const
{
    if (mutableOwner_ == nullptr || !hasEditor() || lineNumber <= 0 || newLines.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = TextEditorTab::tr("Invalid insertion request.");
        }
        return false;
    }

    QStringList trimmedLines;
    trimmedLines.reserve(newLines.size());
    for (const QString &line : newLines) {
        const QString candidate = line.trimmed();
        if (candidate.isEmpty()) {
            continue;
        }
        trimmedLines.append(line);
    }
    if (trimmedLines.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = TextEditorTab::tr("No lines to insert.");
        }
        return false;
    }

    const QString contents = text();
    QStringList lines = blockEditorNormalizedSourceLines(contents);
    const int insertIndex = qBound(0, lineNumber - 1, lines.size());
    for (int offset = 0; offset < trimmedLines.size(); ++offset) {
        lines.insert(insertIndex + offset, trimmedLines.at(offset));
    }

    replaceText(blockEditorJoinSourceLines(contents, lines));
    return true;
}

bool BlockEditorSourceController::removeLineRange(int startLine, int endLine) const
{
    if (mutableOwner_ == nullptr || mutableOwner_->editor_ == nullptr || mutableOwner_->editor_->document() == nullptr
        || startLine <= 0 || endLine < startLine) {
        return false;
    }

    QTextDocument *document = mutableOwner_->editor_->document();
    const QTextBlock startBlock = document->findBlockByLineNumber(startLine - 1);
    const QTextBlock endBlock = document->findBlockByLineNumber(endLine - 1);
    if (!startBlock.isValid() || !endBlock.isValid()) {
        return false;
    }

    const QTextBlock afterEndBlock = endBlock.next();
    const int selectionStart = startBlock.position();
    const int selectionEnd = afterEndBlock.isValid()
        ? afterEndBlock.position()
        : endBlock.position() + qMax(0, endBlock.length() - 1);
    if (selectionEnd < selectionStart) {
        return false;
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    cursor.setPosition(selectionStart);
    cursor.setPosition(selectionEnd, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
    mutableOwner_->editor_->setTextCursor(cursor);
    return true;
}

bool BlockEditorSourceController::replaceLine(int lineNumber, const QString &line) const
{
    if (mutableOwner_ == nullptr || mutableOwner_->editor_ == nullptr) {
        return false;
    }

    const QTextBlock targetBlock = mutableOwner_->editor_->document()->findBlockByLineNumber(lineNumber - 1);
    if (!targetBlock.isValid()) {
        return false;
    }

    QTextCursor editCursor(targetBlock);
    editCursor.beginEditBlock();
    editCursor.movePosition(QTextCursor::StartOfBlock);
    editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editCursor.insertText(line);
    editCursor.endEditBlock();
    mutableOwner_->editor_->setTextCursor(editCursor);
    return true;
}

void BlockEditorSourceController::replaceWithLines(const QString &originalContents, const QStringList &lines) const
{
    replaceText(blockEditorJoinSourceLines(originalContents, lines));
}

void BlockEditorSourceController::replaceText(const QString &contents) const
{
    if (mutableOwner_ != nullptr) {
        mutableOwner_->replaceTextForCommand(contents);
    }
}
}
