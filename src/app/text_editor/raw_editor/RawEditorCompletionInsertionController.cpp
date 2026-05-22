#include "RawEditorCompletionInsertionController.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>

#include <utility>

namespace TherionStudio
{
RawEditorCompletionInsertionController::RawEditorCompletionInsertionController(RawEditorCompletionInsertionContext context)
    : context_(std::move(context))
{
}

void RawEditorCompletionInsertionController::insertCompletionToken(const QString &completion)
{
    if (context_.editor == nullptr || !context_.normalizedDirectiveToken || !context_.closingDirectiveForOpeningToken
        || completion.trimmed().isEmpty()) {
        return;
    }

    QTextCursor cursor = context_.editor->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return;
    }

    const QString blockText = block.text();
    int start = cursor.positionInBlock();
    int end = cursor.positionInBlock();

    auto isCompletionCharacter = [](QChar ch) {
        return ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_');
    };

    while (start > 0 && isCompletionCharacter(blockText.at(start - 1))) {
        --start;
    }
    while (end < blockText.length() && isCompletionCharacter(blockText.at(end))) {
        ++end;
    }

    const QString normalizedCompletion = context_.normalizedDirectiveToken(completion.toLower());
    const QString closingDirective = context_.closingDirectiveForOpeningToken(normalizedCompletion);
    const QString leftTrimmed = blockText.left(start).trimmed();
    const QString rightTrimmed = blockText.mid(end).trimmed();
    const bool firstTokenOnlyLine = leftTrimmed.isEmpty() && rightTrimmed.isEmpty();
    bool shouldInsertClosingPair = !closingDirective.isEmpty() && firstTokenOnlyLine;

    QString lineIndent;
    for (int index = 0; index < blockText.length(); ++index) {
        const QChar ch = blockText.at(index);
        if (!ch.isSpace() || ch == QLatin1Char('\n') || ch == QLatin1Char('\r')) {
            break;
        }
        lineIndent.append(ch);
    }

    if (shouldInsertClosingPair) {
        const QTextBlock nextBlock = block.next();
        if (nextBlock.isValid()) {
            const QString nextText = nextBlock.text().trimmed().toLower();
            if (context_.normalizedDirectiveToken(nextText) == closingDirective) {
                shouldInsertClosingPair = false;
            }
        }
    }

    cursor.beginEditBlock();
    cursor.setPosition(block.position() + start);
    cursor.setPosition(block.position() + end, QTextCursor::KeepAnchor);
    cursor.insertText(completion);
    const int completionEndPos = cursor.position();

    if (shouldInsertClosingPair) {
        cursor.insertText(QStringLiteral("\n%1%2").arg(lineIndent, closingDirective));
        cursor.setPosition(completionEndPos);
    }

    cursor.endEditBlock();
    context_.editor->setTextCursor(cursor);
}
}
