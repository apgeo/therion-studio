#include "RawEditorCompletionPopupController.h"

#include "RawEditorCompletionContextAnalyzer.h"
#include "RawEditorCompletionSuggestionBuilder.h"
#include "../TextEditorCommandMetadata.h"
#include "../TextEditorTab.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDir>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QToolTip>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

namespace TherionStudio
{
RawEditorCompletionPopupController::RawEditorCompletionPopupController(TextEditorTab *owner)
    : owner_(owner)
{
}

void RawEditorCompletionPopupController::triggerCompletionPopup()
{
    if (owner_ == nullptr
        || owner_->editor_ == nullptr
        || owner_->completionCompleter_ == nullptr
        || owner_->completionModel_ == nullptr) {
        return;
    }

    const RawEditorCompletionContextAnalyzer contextAnalyzer(owner_);
    const RawEditorCompletionSuggestionBuilder suggestionBuilder(owner_);
    const QString command = contextAnalyzer.currentCompletionCommand();

    QString rawPathPrefix;
    if (command == QStringLiteral("input")) {
        const QTextCursor cursor = owner_->editor_->textCursor();
        const QTextBlock block = cursor.block();
        if (block.isValid()) {
            const QString blockText = block.text();
            int start = cursor.positionInBlock();
            int end = cursor.positionInBlock();
            while (start > 0 && !blockText.at(start - 1).isSpace()) {
                --start;
            }
            while (end < blockText.length() && !blockText.at(end).isSpace()) {
                ++end;
            }
            rawPathPrefix = QDir::fromNativeSeparators(blockText.mid(start, end - start).trimmed());
        }
    }

    const QString prefix = contextAnalyzer.currentCompletionPrefix();
    QStringList suggestions = suggestionBuilder.buildCompletionSuggestionsForCursor(prefix);
    if (command == QStringLiteral("input") && rawPathPrefix.startsWith(QStringLiteral("./"))) {
        QStringList sameDirectorySuggestions;
        for (const QString &candidate : suggestions) {
            if (!candidate.startsWith(QStringLiteral("../"))) {
                if (!sameDirectorySuggestions.contains(candidate, Qt::CaseInsensitive)) {
                    sameDirectorySuggestions.append(candidate);
                }
            }
        }
        suggestions = sameDirectorySuggestions;
    }

    if (suggestions.isEmpty()) {
        if (owner_->completionCompleter_->popup() != nullptr) {
            owner_->completionCompleter_->popup()->hide();
        }

        const int requiredPositionalCount = qMax(0, owner_->commandMetadata().commandRequiredPositionalCount.value(command));
        if (!command.isEmpty() && requiredPositionalCount > 0) {
            const QTextCursor cursor = owner_->editor_->textCursor();
            const QTextBlock block = cursor.block();
            if (block.isValid()) {
                const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
                const int column = cursor.positionInBlock();

                int tokenIndexAtCursor = parsedLine.tokens.size();
                int tokenCounter = 0;
                for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
                    if (tokenSpan.type == TherionTokenType::Comment) {
                        continue;
                    }
                    const int tokenEnd = tokenSpan.start + tokenSpan.length;
                    if (column >= tokenSpan.start && column <= tokenEnd) {
                        tokenIndexAtCursor = tokenCounter;
                        break;
                    }
                    if (column > tokenEnd) {
                        tokenIndexAtCursor = tokenCounter + 1;
                    }
                    ++tokenCounter;
                }

                const int positionalCountBeforeCursor = positionalTokenCountBeforeCursor(parsedLine, tokenIndexAtCursor);
                if (positionalCountBeforeCursor < requiredPositionalCount) {
                    const int nextArgumentIndex = positionalCountBeforeCursor + 1;
                    QToolTip::showText(owner_->editor_->mapToGlobal(owner_->editor_->cursorRect().bottomLeft()),
                                       owner_->tr("Enter required argument %1 before options.").arg(nextArgumentIndex),
                                       owner_->editor_,
                                       QRect(),
                                       1500);
                }
            }
        }
        return;
    }

    owner_->completionModel_->setStringList(suggestions);
    QString popupPrefix = prefix;
    if (command == QStringLiteral("input")) {
        popupPrefix = suggestionBuilder.normalizeInputCompletionPrefix(rawPathPrefix);
    }
    owner_->completionCompleter_->setCompletionPrefix(popupPrefix);
    if (owner_->completionCompleter_->completionCount() <= 0) {
        return;
    }

    QRect cursorRect = owner_->editor_->cursorRect();
    if (owner_->completionCompleter_->popup() != nullptr) {
        const int popupWidth = owner_->completionCompleter_->popup()->sizeHintForColumn(0)
            + owner_->completionCompleter_->popup()->verticalScrollBar()->sizeHint().width() + 12;
        cursorRect.setWidth(qMax(cursorRect.width(), popupWidth));
    }
    owner_->completionCompleter_->complete(cursorRect);

    const QString scopeLabel = contextAnalyzer.currentCompletionScopeLabel();
    if (!scopeLabel.isEmpty()) {
        QToolTip::showText(owner_->editor_->mapToGlobal(cursorRect.bottomLeft()),
                           owner_->tr("Completion scope: %1").arg(scopeLabel),
                           owner_->editor_,
                           QRect(),
                           1500);
    }
}
}
