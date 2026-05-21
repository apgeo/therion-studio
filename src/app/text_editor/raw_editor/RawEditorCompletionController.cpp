#include "RawEditorCompletionController.h"

#include "RawEditorCompletionContextAnalyzer.h"
#include "RawEditorCommandMetadataLoader.h"
#include "RawEditorCompletionInsertionController.h"
#include "RawEditorCompletionPopupController.h"
#include "RawEditorCompletionSuggestionBuilder.h"
#include "../TextEditorTab.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>

namespace
{
bool isRequiredArgumentSignature(const QString &signature)
{
    const QString trimmed = signature.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.startsWith(QLatin1Char('['))) {
        return false;
    }
    return trimmed.contains(QLatin1Char('<')) && trimmed.contains(QLatin1Char('>'));
}

}

namespace TherionStudio
{
RawEditorCompletionController::RawEditorCompletionController(TextEditorTab *owner)
    : owner_(owner)
{
}

const TextEditorCommandMetadata &RawEditorCompletionController::commandMetadata() const
{
    return owner_->commandMetadata();
}

TextEditorCommandMetadata &RawEditorCompletionController::mutableCommandMetadata() const
{
    return owner_->mutableCommandMetadata();
}

RawEditorCompletionController::EventHandling RawEditorCompletionController::handleEventFilter(QObject *watched,
                                                                                             QEvent *event)
{
    if (owner_ == nullptr || event == nullptr || owner_->completionCompleter_ == nullptr || owner_->editor_ == nullptr) {
        return EventHandling::NotHandled;
    }

    const QObject *popupObject = owner_->completionCompleter_->popup();
    const bool watchedEditor = watched == owner_->editor_ || watched == owner_->editor_->viewport();
    const bool watchedPopup = popupObject != nullptr && watched == popupObject;
    if (!watchedEditor && !watchedPopup) {
        return EventHandling::NotHandled;
    }

    if (watchedEditor
        && (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonDblClick)) {
        if (owner_->completionCompleter_->popup() != nullptr) {
            owner_->completionCompleter_->popup()->hide();
        }

        if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
            && watched == owner_->editor_->viewport()) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                owner_->editor_->setFocus(Qt::MouseFocusReason);
                owner_->editor_->setTextCursor(owner_->editor_->cursorForPosition(mouseEvent->position().toPoint()));
            }
        }

        return EventHandling::PassToBase;
    }

    if (event->type() != QEvent::KeyPress) {
        return EventHandling::PassToBase;
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (owner_->completionCompleter_->popup() != nullptr && owner_->completionCompleter_->popup()->isVisible()) {
        auto *popup = owner_->completionCompleter_->popup();
        if (watchedPopup) {
            const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
            const bool noModifiers = modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier;
            const QString text = keyEvent->text();
            const bool typedText = !text.isEmpty() && noModifiers && text.at(0).isPrint();
            if (typedText) {
                owner_->editor_->setFocus();
                owner_->editor_->insertPlainText(text);
                QTimer::singleShot(0, owner_, [this]() {
                    triggerCompletionPopup();
                });
                return EventHandling::Consumed;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                owner_->editor_->setFocus();
                QTextCursor cursor = owner_->editor_->textCursor();
                cursor.deletePreviousChar();
                owner_->editor_->setTextCursor(cursor);
                QTimer::singleShot(0, owner_, [this]() {
                    triggerCompletionPopup();
                });
                return EventHandling::Consumed;
            }
        }

        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
        {
            QString completion;
            const auto currentIndex = popup->currentIndex();
            if (currentIndex.isValid()) {
                completion = currentIndex.data(Qt::DisplayRole).toString().trimmed();
            }
            if (completion.isEmpty()) {
                completion = owner_->completionCompleter_->currentCompletion().trimmed();
            }
            if (!completion.isEmpty()) {
                insertCompletionToken(completion);
            }
            popup->hide();
            return EventHandling::Consumed;
        }
        case Qt::Key_Escape:
            popup->hide();
            return EventHandling::Consumed;
        default:
            break;
        }
    }

    if (!watchedEditor) {
        return EventHandling::PassToBase;
    }

    const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
    if (keyEvent->key() == Qt::Key_Tab && modifiers == Qt::NoModifier) {
        QTextCursor cursor = owner_->editor_->textCursor();
        cursor.insertText(QStringLiteral("    "));
        owner_->editor_->setTextCursor(cursor);
        return EventHandling::Consumed;
    }

    const bool controlSpace = keyEvent->key() == Qt::Key_Space
        && modifiers.testFlag(Qt::ControlModifier)
        && (modifiers & ~(Qt::ControlModifier)) == Qt::NoModifier;
    if (!controlSpace) {
        const bool noModifiers = modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier;
        if (!noModifiers) {
            return EventHandling::PassToBase;
        }

        const int key = keyEvent->key();
        const bool triggerKey = key == Qt::Key_Backspace
            || key == Qt::Key_Delete
            || key == Qt::Key_Minus
            || key == Qt::Key_Underscore
            || key == Qt::Key_Period
            || key == Qt::Key_Slash
            || key == Qt::Key_Apostrophe
            || key == Qt::Key_QuoteDbl
            || (key >= Qt::Key_A && key <= Qt::Key_Z)
            || (key >= Qt::Key_0 && key <= Qt::Key_9);

        const bool hideKey = key == Qt::Key_Space
            || key == Qt::Key_Tab
            || key == Qt::Key_Backtab
            || key == Qt::Key_Return
            || key == Qt::Key_Enter
            || key == Qt::Key_Escape;

        if (hideKey && owner_->completionCompleter_->popup() != nullptr) {
            owner_->completionCompleter_->popup()->hide();
            return EventHandling::PassToBase;
        }

        if (!triggerKey) {
            return EventHandling::PassToBase;
        }

        const bool deletionKey = key == Qt::Key_Backspace || key == Qt::Key_Delete;
        if (deletionKey) {
            const QTextCursor cursor = owner_->editor_->textCursor();
            const QTextBlock block = cursor.block();
            if (block.isValid()) {
                const QString blockText = block.text();
                const int cursorColumn = cursor.positionInBlock();
                auto isCompletionCharacter = [](const QChar ch) {
                    return ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_');
                };

                const bool leftTokenChar = cursorColumn > 0 && isCompletionCharacter(blockText.at(cursorColumn - 1));
                const bool rightTokenChar = cursorColumn < blockText.size() && isCompletionCharacter(blockText.at(cursorColumn));
                if (!leftTokenChar && !rightTokenChar) {
                    if (owner_->completionCompleter_->popup() != nullptr) {
                        owner_->completionCompleter_->popup()->hide();
                    }
                    return EventHandling::PassToBase;
                }
            }
        }

        QTimer::singleShot(0, owner_, [this]() {
            triggerCompletionPopup();
        });
        return EventHandling::PassToBase;
    }

    triggerCompletionPopup();
    return EventHandling::Consumed;
}

QString RawEditorCompletionController::currentCompletionPrefix() const
{
    return RawEditorCompletionContextAnalyzer(owner_).currentCompletionPrefix();
}

QStringList RawEditorCompletionController::activeCompletionScopeStack() const
{
    return RawEditorCompletionContextAnalyzer(owner_).activeCompletionScopeStack();
}

QString RawEditorCompletionController::currentCompletionCommand() const
{
    return RawEditorCompletionContextAnalyzer(owner_).currentCompletionCommand();
}

QString RawEditorCompletionController::currentCompletionScopeLabel() const
{
    return RawEditorCompletionContextAnalyzer(owner_).currentCompletionScopeLabel();
}

QString RawEditorCompletionController::normalizeCompletionContext(const QString &contextToken) const
{
    return RawEditorCompletionContextAnalyzer(owner_).normalizeCompletionContext(contextToken);
}

bool RawEditorCompletionController::isCompatibleChildKindForBlocks(const QString &parentKind,
                                                                   const QString &childKind) const
{
    return RawEditorCompletionContextAnalyzer(owner_).isCompatibleChildKindForBlocks(parentKind, childKind);
}

bool RawEditorCompletionController::isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const
{
    return RawEditorCompletionContextAnalyzer(owner_).isCommandDirectiveInScope(directive, scopeToken);
}

QStringList RawEditorCompletionController::commandArgumentSignaturesFor(const QString &commandToken) const
{
    if (owner_ == nullptr) {
        return {};
    }
    return mutableCommandMetadata().commandArgumentSignaturesByToken.value(owner_->normalizedDirectiveToken(commandToken.trimmed()));
}

bool RawEditorCompletionController::commandHasRequiredIdArgument(const QString &commandToken) const
{
    const QStringList signatures = commandArgumentSignaturesFor(commandToken);
    for (const QString &signature : signatures) {
        if (!isRequiredArgumentSignature(signature)) {
            continue;
        }
        const QString normalizedSignature = signature.trimmed().toLower();
        return normalizedSignature.contains(QStringLiteral("<id>"));
    }
    return false;
}

bool RawEditorCompletionController::commandSupportsInlineIdField(const QString &commandToken) const
{
    if (owner_ == nullptr) {
        return false;
    }

    const QString normalizedCommand = owner_->normalizedDirectiveToken(commandToken.trimmed());
    if (commandHasRequiredIdArgument(normalizedCommand)) {
        return true;
    }
    return mutableCommandMetadata().commandOptionTokens.value(normalizedCommand).contains(QStringLiteral("-id"), Qt::CaseInsensitive);
}

void RawEditorCompletionController::applyCatalogCommandsMetadata(const QJsonObject &catalogObject) const
{
    RawEditorCommandMetadataLoader(owner_).applyCatalogCommandsMetadata(catalogObject);
}

void RawEditorCompletionController::rebuildCompletionModel() const
{
    RawEditorCommandMetadataLoader(owner_).rebuildCompletionModel();
}

QString RawEditorCompletionController::primaryInsertionScopeForCommand(const QString &commandToken) const
{
    return RawEditorCompletionContextAnalyzer(owner_).primaryInsertionScopeForCommand(commandToken);
}

QString RawEditorCompletionController::resolveScopeForCommandAtLine(const QString &commandToken,
                                                                    const QStringList &lines,
                                                                    int lineNumber) const
{
    return RawEditorCompletionContextAnalyzer(owner_).resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
}

QStringList RawEditorCompletionController::projectInputFileCompletionCandidates() const
{
    return RawEditorCompletionSuggestionBuilder(owner_).projectInputFileCompletionCandidates();
}

QStringList RawEditorCompletionController::buildCompletionSuggestionsForCursor(const QString &prefix) const
{
    return RawEditorCompletionSuggestionBuilder(owner_).buildCompletionSuggestionsForCursor(prefix);
}

void RawEditorCompletionController::triggerCompletionPopup()
{
    RawEditorCompletionPopupController(owner_).triggerCompletionPopup();
}

void RawEditorCompletionController::insertCompletionToken(const QString &completion)
{
    RawEditorCompletionInsertionController(owner_).insertCompletionToken(completion);
}
}
