#include "RawEditorCompletionController.h"

#include "RawEditorCompletionContextAnalyzer.h"
#include "RawEditorCommandMetadataLoader.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>

#include <utility>

namespace TherionStudio
{
RawEditorCompletionController::RawEditorCompletionController(RawEditorCompletionControllerContext context)
    : context_(std::move(context))
{
}

const TextEditorCommandMetadata &RawEditorCompletionController::commandMetadata() const { return *context_.metadata; }

TextEditorCommandMetadata &RawEditorCompletionController::mutableCommandMetadata() const { return *context_.metadata; }

RawEditorCompletionController::EventHandling RawEditorCompletionController::handleEventFilter(QObject *watched,
                                                                                             QEvent *event)
{
    if (event == nullptr
        || context_.eventContext == nullptr
        || context_.completionCompleter == nullptr
        || context_.editor == nullptr) {
        return EventHandling::NotHandled;
    }

    const QObject *popupObject = context_.completionCompleter->popup();
    const bool watchedEditor = watched == context_.editor || watched == context_.editor->viewport();
    const bool watchedPopup = popupObject != nullptr && watched == popupObject;
    if (!watchedEditor && !watchedPopup) {
        return EventHandling::NotHandled;
    }

    if (watchedEditor
        && (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::MouseButtonDblClick)) {
        if (context_.completionCompleter->popup() != nullptr) {
            context_.completionCompleter->popup()->hide();
        }

        if ((event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)
            && watched == context_.editor->viewport()) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                context_.editor->setFocus(Qt::MouseFocusReason);
                context_.editor->setTextCursor(context_.editor->cursorForPosition(mouseEvent->position().toPoint()));
            }
        }

        return EventHandling::PassToBase;
    }

    if (event->type() != QEvent::KeyPress) {
        return EventHandling::PassToBase;
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (context_.completionCompleter->popup() != nullptr && context_.completionCompleter->popup()->isVisible()) {
        auto *popup = context_.completionCompleter->popup();
        if (watchedPopup) {
            const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
            const bool noModifiers = modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier;
            const QString text = keyEvent->text();
            const bool typedText = !text.isEmpty() && noModifiers && text.at(0).isPrint();
            if (typedText) {
                context_.editor->setFocus();
                context_.editor->insertPlainText(text);
                QTimer::singleShot(0, context_.eventContext, [this]() {
                    triggerCompletionPopup();
                });
                return EventHandling::Consumed;
            }
            if (keyEvent->key() == Qt::Key_Backspace) {
                context_.editor->setFocus();
                QTextCursor cursor = context_.editor->textCursor();
                cursor.deletePreviousChar();
                context_.editor->setTextCursor(cursor);
                QTimer::singleShot(0, context_.eventContext, [this]() {
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
                completion = context_.completionCompleter->currentCompletion().trimmed();
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
        QTextCursor cursor = context_.editor->textCursor();
        cursor.insertText(QStringLiteral("    "));
        context_.editor->setTextCursor(cursor);
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

        if (hideKey && context_.completionCompleter->popup() != nullptr) {
            context_.completionCompleter->popup()->hide();
            return EventHandling::PassToBase;
        }

        if (!triggerKey) {
            return EventHandling::PassToBase;
        }

        const bool deletionKey = key == Qt::Key_Backspace || key == Qt::Key_Delete;
        if (deletionKey) {
            const QTextCursor cursor = context_.editor->textCursor();
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
                    if (context_.completionCompleter->popup() != nullptr) {
                        context_.completionCompleter->popup()->hide();
                    }
                    return EventHandling::PassToBase;
                }
            }
        }

        QTimer::singleShot(0, context_.eventContext, [this]() {
            triggerCompletionPopup();
        });
        return EventHandling::PassToBase;
    }

    triggerCompletionPopup();
    return EventHandling::Consumed;
}

QString RawEditorCompletionController::currentCompletionPrefix() const
{
    return contextAnalyzer().currentCompletionPrefix();
}

QStringList RawEditorCompletionController::activeCompletionScopeStack() const
{
    return contextAnalyzer().activeCompletionScopeStack();
}

QString RawEditorCompletionController::currentCompletionCommand() const
{
    return contextAnalyzer().currentCompletionCommand();
}

QString RawEditorCompletionController::currentCompletionScopeLabel() const
{
    return contextAnalyzer().currentCompletionScopeLabel();
}

QString RawEditorCompletionController::normalizeCompletionContext(const QString &contextToken) const
{
    return contextAnalyzer().normalizeCompletionContext(contextToken);
}

bool RawEditorCompletionController::isCompatibleChildKindForBlocks(const QString &parentKind,
                                                                   const QString &childKind) const
{
    return contextAnalyzer().isCompatibleChildKindForBlocks(parentKind, childKind);
}

bool RawEditorCompletionController::isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const
{
    return contextAnalyzer().isCommandDirectiveInScope(directive, scopeToken);
}

void RawEditorCompletionController::applyCatalogCommandsMetadata(const QJsonObject &catalogObject) const
{
    if (context_.metadata != nullptr && context_.completionModel != nullptr) {
        RawEditorCommandMetadataLoader({&mutableCommandMetadata(), context_.completionModel, context_.normalizedDirectiveToken})
            .applyCatalogCommandsMetadata(catalogObject);
    }
}
void RawEditorCompletionController::rebuildCompletionModel() const
{
    if (context_.metadata != nullptr && context_.completionModel != nullptr) {
        RawEditorCommandMetadataLoader({&mutableCommandMetadata(), context_.completionModel, {}}).rebuildCompletionModel();
    }
}
QString RawEditorCompletionController::primaryInsertionScopeForCommand(const QString &commandToken) const
{
    return contextAnalyzer().primaryInsertionScopeForCommand(commandToken);
}

QString RawEditorCompletionController::resolveScopeForCommandAtLine(const QString &commandToken,
                                                                    const QStringList &lines,
                                                                    int lineNumber) const
{
    return contextAnalyzer().resolveScopeForCommandAtLine(commandToken, lines, lineNumber);
}

}
