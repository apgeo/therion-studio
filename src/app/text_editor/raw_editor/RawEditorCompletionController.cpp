#include "RawEditorCompletionController.h"

#include "../TextEditorTab.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QStringListModel>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QToolTip>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

#include <algorithm>

namespace
{
void appendUnique(QStringList &target, const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (target.contains(trimmed, Qt::CaseInsensitive)) {
        return;
    }
    target.append(trimmed);
}

void appendUniqueList(QStringList &target, const QStringList &values)
{
    for (const QString &value : values) {
        appendUnique(target, value);
    }
}

QString normalizeInputCompletionPrefix(QString prefix)
{
    prefix = QDir::fromNativeSeparators(prefix).trimmed();
    if (prefix.isEmpty()) {
        return prefix;
    }

    while (prefix.startsWith(QStringLiteral("./"))) {
        prefix.remove(0, 2);
    }
    return prefix;
}

bool isActivatedInputPathPrefix(const QString &rawPrefix)
{
    const QString normalized = QDir::fromNativeSeparators(rawPrefix).trimmed();
    return normalized.startsWith(QStringLiteral("./")) || normalized.startsWith(QStringLiteral("../"));
}

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

QString normalizedCompletionContextToken(const QString &token)
{
    QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("centreline")) {
        normalized = QStringLiteral("centerline");
    }
    if (normalized == QStringLiteral("endcentreline")) {
        normalized = QStringLiteral("endcenterline");
    }
    if (normalized == QStringLiteral("all")) {
        return QStringLiteral("all");
    }
    if (normalized == QStringLiteral("none")) {
        return QStringLiteral("none");
    }
    static const QRegularExpression contextTokenPattern(QStringLiteral("^[a-z][a-z0-9-]*$"));
    if (contextTokenPattern.match(normalized).hasMatch()) {
        return normalized;
    }
    return QString();
}

QString normalizeInputSuggestionPath(QString path)
{
    path = QDir::fromNativeSeparators(path).trimmed();
    if (path.isEmpty() || path == QStringLiteral(".")) {
        return QString();
    }
    while (path.startsWith(QStringLiteral("./"))) {
        path.remove(0, 2);
        path = path.trimmed();
    }
    return path;
}

QStringList optionArgumentLabelsFromSignature(const QString &signature)
{
    QStringList labels;
    static const QRegularExpression placeholderPattern(QStringLiteral(R"(<([^>]+)>)"));
    QRegularExpressionMatchIterator iterator = placeholderPattern.globalMatch(signature);
    while (iterator.hasNext()) {
        const QRegularExpressionMatch match = iterator.next();
        const QString placeholder = match.captured(1).trimmed();
        if (placeholder.isEmpty()) {
            continue;
        }
        QString label = placeholder.toLower();
        QStringList words = label.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (QString &word : words) {
            if (!word.isEmpty()) {
                word[0] = word.at(0).toUpper();
            }
        }
        label = words.join(QLatin1Char(' '));
        if (!label.isEmpty()) {
            labels.append(label);
        }
    }
    return labels;
}
}

namespace TherionStudio
{
RawEditorCompletionController::RawEditorCompletionController(TextEditorTab *owner)
    : owner_(owner)
{
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
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return QString();
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
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

    return blockText.mid(start, end - start).trimmed();
}

QStringList RawEditorCompletionController::activeCompletionScopeStack() const
{
    QStringList scopeStack;
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return scopeStack;
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    const int currentBlockNumber = cursor.block().blockNumber();
    if (currentBlockNumber <= 0) {
        return scopeStack;
    }

    const QStringList lines = owner_->editor_->toPlainText().split(QLatin1Char('\n'));
    const int lastLine = qMin(currentBlockNumber, lines.size());
    for (int lineIndex = 0; lineIndex < lastLine; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        if (parsedLine.directive.isEmpty()) {
            continue;
        }

        const QString directive = owner_->normalizedDirectiveToken(parsedLine.directive);
        const QString openingFromClosing = owner_->openingDirectiveForClosingToken(directive);
        if (!openingFromClosing.isEmpty()) {
            for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (scopeStack.at(stackIndex) == openingFromClosing) {
                    scopeStack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }

        if (owner_->isContainerDirectiveInstanceForParsedLine(directive, parsedLine)) {
            scopeStack.append(directive);
        }
    }

    return scopeStack;
}

QString RawEditorCompletionController::currentCompletionCommand() const
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return QString();
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = owner_->normalizedDirectiveToken(parsedLine.directive.toLower());
    if (owner_->commandCompletionTokens_.contains(directive, Qt::CaseInsensitive)
        || owner_->commandOptionTokens_.contains(directive)
        || owner_->commandValueTokens_.contains(directive)) {
        return directive;
    }

    const QStringList scopeStack = activeCompletionScopeStack();
    for (int index = scopeStack.size() - 1; index >= 0; --index) {
        const QString scopeDirective = scopeStack.at(index);
        if (owner_->commandCompletionTokens_.contains(scopeDirective, Qt::CaseInsensitive)) {
            return scopeDirective;
        }
    }
    return QString();
}

QString RawEditorCompletionController::currentCompletionScopeLabel() const
{
    const QStringList scopeStack = activeCompletionScopeStack();
    if (scopeStack.isEmpty()) {
        return owner_ != nullptr ? owner_->tr("top-level") : QStringLiteral("top-level");
    }

    return scopeStack.constLast();
}

QString RawEditorCompletionController::normalizeCompletionContext(const QString &contextToken) const
{
    return normalizedCompletionContextToken(contextToken);
}

bool RawEditorCompletionController::isCompatibleChildKindForBlocks(const QString &parentKind,
                                                                   const QString &childKind) const
{
    if (owner_ == nullptr) {
        return false;
    }

    const QString normalizedParent = owner_->normalizedDirectiveToken(parentKind.trimmed().toLower());
    const QString normalizedChild = owner_->normalizedDirectiveToken(childKind.trimmed().toLower());
    if (normalizedChild.isEmpty()) {
        return false;
    }
    if (normalizedChild == QStringLiteral("encoding") || normalizedParent == QStringLiteral("encoding")) {
        return false;
    }
    if (normalizedChild == QStringLiteral("comment")) {
        return true;
    }

    const QString parentContext = normalizedParent.isEmpty()
        ? QStringLiteral("none")
        : normalizeCompletionContext(normalizedParent);
    const QStringList childContexts = owner_->blockCommandContextsByKind_.value(normalizedChild);
    if (!childContexts.isEmpty()) {
        if (childContexts.contains(QStringLiteral("all"), Qt::CaseInsensitive)) {
            return true;
        }
        if (!parentContext.isEmpty() && childContexts.contains(parentContext, Qt::CaseInsensitive)) {
            return true;
        }
        return false;
    }

    return normalizedParent.isEmpty();
}

bool RawEditorCompletionController::isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const
{
    if (owner_ == nullptr) {
        return false;
    }

    const QString normalizedDirective = owner_->normalizedDirectiveToken(directive.trimmed());
    if (normalizedDirective.isEmpty()) {
        return false;
    }

    QString normalizedScope = normalizeCompletionContext(scopeToken);
    if (normalizedScope.isEmpty()) {
        normalizedScope = QStringLiteral("none");
    }

    QStringList candidates = owner_->contextCommandTokens_.value(normalizedScope);
    appendUniqueList(candidates, owner_->contextCommandTokens_.value(QStringLiteral("all")));
    if (normalizedScope == QStringLiteral("none")) {
        appendUniqueList(candidates, owner_->contextCommandTokens_.value(QStringLiteral("none")));
    }
    return candidates.contains(normalizedDirective, Qt::CaseInsensitive);
}

QStringList RawEditorCompletionController::commandArgumentSignaturesFor(const QString &commandToken) const
{
    if (owner_ == nullptr) {
        return {};
    }
    return owner_->commandArgumentSignaturesByToken_.value(owner_->normalizedDirectiveToken(commandToken.trimmed()));
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
    return owner_->commandOptionTokens_.value(normalizedCommand).contains(QStringLiteral("-id"), Qt::CaseInsensitive);
}

void RawEditorCompletionController::applyCommandArgumentMetadata(const QString &commandName,
                                                                 const QJsonObject &commandObject,
                                                                 TherionHelpEntry *entry,
                                                                 int *requiredPositionalCount,
                                                                 bool *primaryValueIsPerson,
                                                                 QStringList *commandArgumentSignatures) const
{
    if (owner_ == nullptr || entry == nullptr || requiredPositionalCount == nullptr || primaryValueIsPerson == nullptr
        || commandArgumentSignatures == nullptr) {
        return;
    }

    const QJsonArray argumentsArray = commandObject.value(QStringLiteral("arguments")).toArray();
    for (int argumentIndex = 0; argumentIndex < argumentsArray.size(); ++argumentIndex) {
        const QJsonValue value = argumentsArray.at(argumentIndex);
        const QJsonObject argumentObject = value.toObject();
        const QString signature = argumentObject.value(QStringLiteral("signature")).toString().trimmed();
        if (!signature.isEmpty()) {
            commandArgumentSignatures->append(signature);
        }
        const QString description = argumentObject.value(QStringLiteral("description")).toString().trimmed();
        const QString argumentLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
        appendUnique(entry->arguments, argumentLine);
        if (isRequiredArgumentSignature(signature)) {
            ++(*requiredPositionalCount);
        }
        if (!(*primaryValueIsPerson)) {
            const QString normalizedSignature = signature.trimmed().toLower();
            if (normalizedSignature.contains(QStringLiteral("<person>"))) {
                *primaryValueIsPerson = true;
            }
        }

        QStringList argumentAllowedValues;
        const QJsonArray argumentAllowedValuesArray = argumentObject.value(QStringLiteral("allowed_values")).toArray();
        for (const QJsonValue &argumentAllowedValue : argumentAllowedValuesArray) {
            appendUnique(argumentAllowedValues, argumentAllowedValue.toString().trimmed().toLower());
        }
        if (!argumentAllowedValues.isEmpty()) {
            appendUniqueList(owner_->commandArgumentValueTokens_[commandArgumentValueKey(commandName, argumentIndex)],
                             argumentAllowedValues);
        }
    }
}

void RawEditorCompletionController::applyCommandContextMetadata(const QString &commandName,
                                                                const QJsonObject &commandObject,
                                                                QStringList *normalizedCommandContexts) const
{
    if (owner_ == nullptr || normalizedCommandContexts == nullptr) {
        return;
    }

    normalizedCommandContexts->clear();
    const QJsonArray contextsArray = commandObject.value(QStringLiteral("contexts")).toArray();
    for (const QJsonValue &contextValue : contextsArray) {
        const QString contextToken = normalizeCompletionContext(contextValue.toString());
        if (contextToken.isEmpty()) {
            continue;
        }
        appendUnique(*normalizedCommandContexts, contextToken);
        appendUnique(owner_->contextCommandTokens_[contextToken], commandName);
    }
    appendUniqueList(owner_->blockCommandContextsByKind_[owner_->normalizedDirectiveToken(commandName)],
                     *normalizedCommandContexts);

    QStringList inlineCommands;
    const QJsonArray inlineCommandArray = commandObject.value(QStringLiteral("inline_commands")).toArray();
    for (const QJsonValue &inlineCommandValue : inlineCommandArray) {
        appendUnique(inlineCommands, inlineCommandValue.toString());
    }
    if (inlineCommands.isEmpty()) {
        return;
    }

    QStringList targetContexts = *normalizedCommandContexts;
    if (targetContexts.isEmpty()) {
        appendUnique(targetContexts, QStringLiteral("all"));
    }
    for (const QString &inlineCommand : inlineCommands) {
        const QString normalizedInlineCommand = owner_->normalizedDirectiveToken(inlineCommand);
        if (normalizedInlineCommand.isEmpty()) {
            continue;
        }
        for (const QString &contextToken : targetContexts) {
            appendUnique(owner_->contextCommandTokens_[contextToken], normalizedInlineCommand);
        }
        registerCompletionToken(inlineCommand);
    }
}

void RawEditorCompletionController::applyCommandOptionCatalogMetadata(const QString &commandName,
                                                                      const QJsonObject &commandObject,
                                                                      TherionHelpEntry *entry) const
{
    if (owner_ == nullptr || entry == nullptr) {
        return;
    }

    const QJsonArray optionsArray = commandObject.value(QStringLiteral("options")).toArray();
    for (const QJsonValue &value : optionsArray) {
        const QJsonObject optionObject = value.toObject();
        const QString signature = optionObject.value(QStringLiteral("signature")).toString().trimmed();
        const QString description = optionObject.value(QStringLiteral("description")).toString().trimmed();
        const QString optionKey = optionObject.value(QStringLiteral("option_key")).toString().trimmed();
        const QString valueArity = canonicalOptionArityToken(
            optionObject.value(QStringLiteral("value_arity")).toString());
        const QString optionLine = description.isEmpty() ? signature : QStringLiteral("%1 = %2").arg(signature, description);
        appendUnique(entry->options, optionLine);
        const QStringList normalizedOptionKeys = extractOptionKeys(optionKey);
        const QStringList optionArgumentLabels = optionArgumentLabelsFromSignature(signature);
        const bool signatureHasEllipsis = signature.contains(QStringLiteral("..."));
        int fixedArity = -1;
        if (valueArity == QStringLiteral("NONE")) {
            fixedArity = 0;
        } else if (valueArity == QStringLiteral("EXACTLY_ONE")) {
            fixedArity = 1;
        } else if (valueArity == QStringLiteral("ONE_OR_MORE")
                   && !signatureHasEllipsis
                   && optionArgumentLabels.size() >= 2) {
            fixedArity = optionArgumentLabels.size();
        }
        QStringList normalizedOptionValues;
        for (const QString &normalizedOptionKey : normalizedOptionKeys) {
            appendUnique(entry->relatedKeywords, normalizedOptionKey);
            appendUnique(owner_->commandOptionTokens_[commandName], normalizedOptionKey);
            if (!valueArity.isEmpty()) {
                owner_->commandOptionValueArityTokens_.insert(commandOptionValueKey(commandName, normalizedOptionKey), valueArity);
            }
            if (!optionArgumentLabels.isEmpty()) {
                owner_->commandOptionArgumentLabelsByKey_.insert(commandOptionValueKey(commandName, normalizedOptionKey),
                                                                 optionArgumentLabels);
            }
            if (fixedArity >= 0) {
                owner_->commandOptionFixedArityByKey_.insert(commandOptionValueKey(commandName, normalizedOptionKey),
                                                             fixedArity);
            }
            registerCompletionToken(normalizedOptionKey);
        }
        const QJsonArray optionValues = optionObject.value(QStringLiteral("allowed_values")).toArray();
        for (const QJsonValue &optionValue : optionValues) {
            const QString normalizedValue = optionValue.toString().trimmed();
            appendUnique(normalizedOptionValues, normalizedValue);
        }
        QString optionHelpHtml;
        {
            QStringList html;
            html << QStringLiteral("<p><b>Option:</b> %1</p>").arg(signature.toHtmlEscaped());
            if (!description.isEmpty()) {
                html << QStringLiteral("<p><b>Description:</b> %1</p>").arg(description.toHtmlEscaped());
            }
            if (!valueArity.isEmpty()) {
                html << QStringLiteral("<p><b>Value Arity:</b> %1</p>").arg(valueArity.toHtmlEscaped());
            }
            if (!normalizedOptionValues.isEmpty()) {
                html << QStringLiteral("<p><b>Accepted Values:</b> %1</p>")
                            .arg(normalizedOptionValues.join(QStringLiteral(", ")).toHtmlEscaped());
            }
            optionHelpHtml = html.join(QString());
        }
        if (!normalizedOptionValues.isEmpty()) {
            for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                appendUniqueList(owner_->commandOptionValueTokens_[commandOptionValueKey(commandName, normalizedOptionKey)],
                                 normalizedOptionValues);
            }
        }
        if (!optionHelpHtml.isEmpty()) {
            for (const QString &normalizedOptionKey : normalizedOptionKeys) {
                owner_->commandOptionHelpHtmlByKey_.insert(commandOptionHelpKey(commandName, normalizedOptionKey), optionHelpHtml);
            }
        }
    }

    const QJsonArray allowedValuesArray = commandObject.value(QStringLiteral("allowed_values")).toArray();
    for (const QJsonValue &value : allowedValuesArray) {
        appendUnique(entry->acceptedValues, value.toString());
    }

    const QJsonArray typeValuesArray = commandObject.value(QStringLiteral("type_values")).toArray();
    for (const QJsonValue &value : typeValuesArray) {
        appendUnique(owner_->commandTypeValueTokens_[commandName], value.toString().trimmed().toLower());
    }

    const QJsonObject subtypeByTypeObject = commandObject.value(QStringLiteral("subtype_by_type")).toObject();
    for (auto subtypeIterator = subtypeByTypeObject.begin(); subtypeIterator != subtypeByTypeObject.end(); ++subtypeIterator) {
        const QString typeKey = subtypeIterator.key().trimmed().toLower();
        if (typeKey.isEmpty()) {
            continue;
        }

        const QJsonArray subtypeArray = subtypeIterator.value().toArray();
        for (const QJsonValue &subtypeValue : subtypeArray) {
            appendUnique(owner_->commandSubtypeByTypeTokens_[commandName][typeKey],
                         subtypeValue.toString().trimmed().toLower());
        }
    }
}

void RawEditorCompletionController::applyCommandRegistrationMetadata(const QString &commandName,
                                                                     const TherionHelpEntry &entry,
                                                                     int requiredPositionalCount,
                                                                     bool primaryValueIsPerson,
                                                                     const QStringList &commandArgumentSignatures) const
{
    if (owner_ == nullptr) {
        return;
    }

    appendUnique(owner_->commandCompletionTokens_, commandName);
    owner_->commandRequiredPositionalCount_.insert(commandName, requiredPositionalCount);
    if (!commandArgumentSignatures.isEmpty()) {
        owner_->commandArgumentSignaturesByToken_.insert(commandName, commandArgumentSignatures);
    }
    owner_->commandPrimaryValueIsPerson_.insert(commandName, primaryValueIsPerson);
    registerCompletionToken(commandName);
    for (const QString &keyword : entry.relatedKeywords) {
        registerCompletionToken(keyword);
    }
    for (const QString &acceptedValue : entry.acceptedValues) {
        registerCompletionToken(acceptedValue);
        appendUnique(owner_->commandValueTokens_[commandName], acceptedValue);
    }

    mergeHelpEntry(commandName, entry);
}

void RawEditorCompletionController::applyCommandAliasMetadata(const QString &commandName,
                                                              const QJsonObject &commandObject,
                                                              const TherionHelpEntry &entry,
                                                              const QStringList &normalizedCommandContexts) const
{
    if (owner_ == nullptr) {
        return;
    }

    const QJsonArray aliasesArray = commandObject.value(QStringLiteral("aliases")).toArray();
    for (const QJsonValue &aliasValue : aliasesArray) {
        const QString alias = aliasValue.toString().trimmed().toLower();
        if (alias.isEmpty()) {
            continue;
        }
        appendUnique(owner_->commandCompletionTokens_, alias);
        owner_->commandRequiredPositionalCount_.insert(alias, owner_->commandRequiredPositionalCount_.value(commandName));
        appendUniqueList(owner_->commandArgumentSignaturesByToken_[alias],
                         owner_->commandArgumentSignaturesByToken_.value(commandName));
        owner_->commandPrimaryValueIsPerson_.insert(alias, owner_->commandPrimaryValueIsPerson_.value(commandName));
        registerCompletionToken(alias);
        TherionHelpEntry aliasEntry = entry;
        appendUnique(aliasEntry.relatedKeywords, commandName);
        mergeHelpEntry(alias, aliasEntry);
        appendUniqueList(owner_->commandOptionTokens_[alias], owner_->commandOptionTokens_.value(commandName));
        appendUniqueList(owner_->commandValueTokens_[alias], owner_->commandValueTokens_.value(commandName));
        const QString argumentPrefix = commandName + QStringLiteral("\x1farg\x1f");
        QStringList commandArgumentKeys;
        for (auto argumentIterator = owner_->commandArgumentValueTokens_.cbegin();
             argumentIterator != owner_->commandArgumentValueTokens_.cend();
             ++argumentIterator) {
            if (argumentIterator.key().startsWith(argumentPrefix)) {
                commandArgumentKeys.append(argumentIterator.key());
            }
        }
        for (const QString &key : commandArgumentKeys) {
            const QString suffix = key.mid(commandName.size());
            appendUniqueList(owner_->commandArgumentValueTokens_[alias + suffix],
                             owner_->commandArgumentValueTokens_.value(key));
        }
        appendUniqueList(owner_->commandTypeValueTokens_[alias], owner_->commandTypeValueTokens_.value(commandName));
        for (const QString &optionKey : owner_->commandOptionTokens_.value(commandName)) {
            appendUniqueList(owner_->commandOptionValueTokens_[commandOptionValueKey(alias, optionKey)],
                             owner_->commandOptionValueTokens_.value(commandOptionValueKey(commandName, optionKey)));
            const QString key = commandOptionValueKey(commandName, optionKey);
            const QString aliasKey = commandOptionValueKey(alias, optionKey);
            const QString valueArity = owner_->commandOptionValueArityTokens_.value(key);
            if (!valueArity.isEmpty()) {
                owner_->commandOptionValueArityTokens_.insert(aliasKey, valueArity);
            }
            const QStringList optionArgumentLabels = owner_->commandOptionArgumentLabelsByKey_.value(key);
            if (!optionArgumentLabels.isEmpty()) {
                owner_->commandOptionArgumentLabelsByKey_.insert(aliasKey, optionArgumentLabels);
            }
            const int fixedArity = owner_->commandOptionFixedArityByKey_.value(key, -1);
            if (fixedArity >= 0) {
                owner_->commandOptionFixedArityByKey_.insert(aliasKey, fixedArity);
            }
            const QString optionHelpHtml = owner_->commandOptionHelpHtmlByKey_.value(commandOptionHelpKey(commandName, optionKey));
            if (!optionHelpHtml.isEmpty()) {
                owner_->commandOptionHelpHtmlByKey_.insert(commandOptionHelpKey(alias, optionKey), optionHelpHtml);
            }
        }
        const QHash<QString, QStringList> subtypeByType = owner_->commandSubtypeByTypeTokens_.value(commandName);
        for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
            appendUniqueList(owner_->commandSubtypeByTypeTokens_[alias][subtypeIterator.key()], subtypeIterator.value());
        }
        for (auto contextIterator = owner_->contextCommandTokens_.begin();
             contextIterator != owner_->contextCommandTokens_.end();
             ++contextIterator) {
            if (contextIterator.value().contains(commandName, Qt::CaseInsensitive)) {
                appendUnique(contextIterator.value(), alias);
            }
        }
        appendUniqueList(owner_->blockCommandContextsByKind_[owner_->normalizedDirectiveToken(alias)],
                         normalizedCommandContexts);
    }
}

void RawEditorCompletionController::applyCatalogCommandsMetadata(const QJsonObject &catalogObject) const
{
    if (owner_ == nullptr || catalogObject.isEmpty()) {
        return;
    }

    const QJsonArray commands = catalogObject.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commands) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (commandName.isEmpty()) {
            continue;
        }

        TherionHelpEntry entry;
        entry.summary = commandObject.value(QStringLiteral("summary")).toString().trimmed();
        int requiredPositionalCount = 0;
        QStringList commandArgumentSignatures;
        bool primaryValueIsPerson = false;

        const QJsonArray syntaxArray = commandObject.value(QStringLiteral("syntax")).toArray();
        QStringList syntaxRows;
        for (const QJsonValue &value : syntaxArray) {
            appendUnique(syntaxRows, value.toString());
        }
        entry.syntax = syntaxRows.join(QStringLiteral("\n"));

        applyCommandArgumentMetadata(commandName,
                                     commandObject,
                                     &entry,
                                     &requiredPositionalCount,
                                     &primaryValueIsPerson,
                                     &commandArgumentSignatures);

        applyCommandOptionCatalogMetadata(commandName, commandObject, &entry);
        appendUnique(entry.relatedKeywords, commandName);

        QStringList normalizedCommandContexts;
        applyCommandContextMetadata(commandName, commandObject, &normalizedCommandContexts);
        applyCommandRegistrationMetadata(commandName,
                                         entry,
                                         requiredPositionalCount,
                                         primaryValueIsPerson,
                                         commandArgumentSignatures);
        applyCommandAliasMetadata(commandName, commandObject, entry, normalizedCommandContexts);
    }
}

void RawEditorCompletionController::mergeHelpEntry(const QString &token, const TherionHelpEntry &entry) const
{
    if (owner_ == nullptr || token.trimmed().isEmpty()) {
        return;
    }

    const QString normalizedToken = token.toLower();
    TherionHelpEntry merged = owner_->helpEntries_.value(normalizedToken);
    if (merged.summary.trimmed().isEmpty() && !entry.summary.trimmed().isEmpty()) {
        merged.summary = entry.summary.trimmed();
    }
    if (merged.syntax.trimmed().isEmpty() && !entry.syntax.trimmed().isEmpty()) {
        merged.syntax = entry.syntax.trimmed();
    }
    appendUniqueList(merged.arguments, entry.arguments);
    appendUniqueList(merged.acceptedValues, entry.acceptedValues);
    appendUniqueList(merged.options, entry.options);
    appendUniqueList(merged.relatedKeywords, entry.relatedKeywords);
    owner_->helpEntries_.insert(normalizedToken, merged);
}

void RawEditorCompletionController::registerCompletionToken(const QString &token) const
{
    if (owner_ == nullptr) {
        return;
    }
    const QString normalized = token.trimmed();
    if (normalized.isEmpty()) {
        return;
    }
    if (normalized.startsWith(QLatin1Char('<')) && normalized.endsWith(QLatin1Char('>'))) {
        return;
    }
    if (normalized.contains(QLatin1Char(' '))) {
        return;
    }
    if (owner_->completionTokens_.contains(normalized, Qt::CaseInsensitive)) {
        return;
    }
    owner_->completionTokens_.append(normalized);
}

void RawEditorCompletionController::rebuildCompletionModel() const
{
    if (owner_ == nullptr || owner_->completionModel_ == nullptr) {
        return;
    }

    QStringList sortedTokens = owner_->completionTokens_;
    std::sort(sortedTokens.begin(),
              sortedTokens.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });
    owner_->completionModel_->setStringList(sortedTokens);
}

QString RawEditorCompletionController::primaryInsertionScopeForCommand(const QString &commandToken) const
{
    if (owner_ == nullptr) {
        return QString();
    }

    const QString normalizedCommand = owner_->normalizedDirectiveToken(commandToken.trimmed());
    const QStringList contexts = owner_->blockCommandContextsByKind_.value(normalizedCommand);
    for (const QString &context : contexts) {
        const QString normalizedContext = normalizeCompletionContext(context);
        if (normalizedContext.isEmpty()
            || normalizedContext == QStringLiteral("all")
            || normalizedContext == QStringLiteral("none")) {
            continue;
        }
        return normalizedContext;
    }
    return QString();
}

QString RawEditorCompletionController::resolveScopeForCommandAtLine(const QString &commandToken,
                                                                    const QStringList &lines,
                                                                    int lineNumber) const
{
    if (owner_ == nullptr) {
        return QString();
    }

    const QString normalizedCommand = owner_->normalizedDirectiveToken(commandToken.trimmed());
    if (normalizedCommand.isEmpty()) {
        return QString();
    }

    const QString preferredScope = primaryInsertionScopeForCommand(normalizedCommand);
    if (!preferredScope.isEmpty()) {
        return preferredScope;
    }

    const int lastLine = qBound(0, lineNumber - 1, lines.size());
    QStringList scopeStack;
    for (int lineIndex = 0; lineIndex < lastLine; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        const QString directive = owner_->normalizedDirectiveToken(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        const QString openingFromClosing = owner_->openingDirectiveForClosingToken(directive);
        if (!openingFromClosing.isEmpty()) {
            for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
                if (scopeStack.at(stackIndex) == openingFromClosing) {
                    scopeStack.removeAt(stackIndex);
                    break;
                }
            }
            continue;
        }

        if (owner_->isContainerDirectiveInstanceForParsedLine(directive, parsedLine)) {
            scopeStack.append(directive);
        }
    }

    for (int stackIndex = scopeStack.size() - 1; stackIndex >= 0; --stackIndex) {
        const QString scopeDirective = scopeStack.at(stackIndex);
        if (isCommandDirectiveInScope(normalizedCommand, scopeDirective)) {
            return scopeDirective;
        }
    }

    if (isCommandDirectiveInScope(normalizedCommand, QStringLiteral("none"))) {
        return QStringLiteral("none");
    }

    return QString();
}

QStringList RawEditorCompletionController::projectInputFileCompletionCandidates() const
{
    if (owner_ == nullptr) {
        return {};
    }

    QString rootPath = owner_->projectRootPath_.trimmed();
    if (rootPath.isEmpty()) {
        if (owner_->filePath_.isEmpty()) {
            return {};
        }
        rootPath = QFileInfo(owner_->filePath_).absolutePath();
    }

    QDir rootDir(rootPath);
    if (!rootDir.exists()) {
        return {};
    }

    QString rootBasePath = QFileInfo(rootDir.absolutePath()).canonicalFilePath();
    if (rootBasePath.isEmpty()) {
        rootBasePath = rootDir.absolutePath();
    }
    QDir rootBaseDir(rootBasePath);

    QString baseDirPath = rootBasePath;
    if (!owner_->filePath_.isEmpty()) {
        const QFileInfo currentFileInfo(owner_->filePath_);
        QString candidateBasePath = currentFileInfo.absolutePath();
        const QString canonicalCandidateBasePath = QFileInfo(candidateBasePath).canonicalFilePath();
        if (!canonicalCandidateBasePath.isEmpty()) {
            candidateBasePath = canonicalCandidateBasePath;
        }
        if (QDir(candidateBasePath).exists()) {
            baseDirPath = candidateBasePath;
        }
    }

    QDir baseDir(baseDirPath);

    QStringList candidates;
    QDirIterator iterator(rootBaseDir.absolutePath(),
                          QDir::Files | QDir::Readable | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString absolutePath = iterator.next();
        const QFileInfo fileInfo(absolutePath);
        const QString suffix = fileInfo.suffix().trimmed().toLower();
        if (suffix != QStringLiteral("th") && suffix != QStringLiteral("th2")) {
            continue;
        }

        QString candidatePath = fileInfo.absoluteFilePath();
        const QString canonicalCandidatePath = fileInfo.canonicalFilePath();
        if (!canonicalCandidatePath.isEmpty()) {
            candidatePath = canonicalCandidatePath;
        }

        const QString projectRelativePath = QDir::fromNativeSeparators(rootBaseDir.relativeFilePath(candidatePath)).trimmed();
        if (projectRelativePath.isEmpty() || projectRelativePath.startsWith(QStringLiteral("../"))) {
            continue;
        }

        const QString relativePath = normalizeInputSuggestionPath(baseDir.relativeFilePath(candidatePath));
        if (relativePath.isEmpty()) {
            continue;
        }
        appendUnique(candidates, relativePath);
    }

    return candidates;
}

QStringList RawEditorCompletionController::buildCompletionSuggestionsForCursor(const QString &prefix) const
{
    if (owner_ == nullptr || owner_->editor_ == nullptr) {
        return QStringList();
    }

    const QTextCursor cursor = owner_->editor_->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QStringList();
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(block.text(), block.blockNumber() + 1);
    const int column = cursor.positionInBlock();

    int tokenIndexAtCursor = parsedLine.tokens.size();
    bool cursorInsideToken = false;
    int tokenCounter = 0;
    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }
        const int tokenEnd = tokenSpan.start + tokenSpan.length;
        if (column >= tokenSpan.start && column <= tokenEnd) {
            tokenIndexAtCursor = tokenCounter;
            cursorInsideToken = true;
            break;
        }
        if (column > tokenEnd) {
            tokenIndexAtCursor = tokenCounter + 1;
        }
        ++tokenCounter;
    }

    const QString currentToken = cursorInsideToken && tokenIndexAtCursor < parsedLine.tokens.size()
        ? parsedLine.tokens.at(tokenIndexAtCursor)
        : QString();
    const QString previousToken = tokenIndexAtCursor > 0 && tokenIndexAtCursor - 1 < parsedLine.tokens.size()
        ? parsedLine.tokens.at(tokenIndexAtCursor - 1)
        : QString();
    QString effectivePrefix = !prefix.isEmpty()
        ? prefix
        : (cursorInsideToken ? currentToken.trimmed() : QString());

    const QString command = currentCompletionCommand();
    int tokenStart = column;
    int tokenEnd = column;
    const QString blockText = block.text();
    while (tokenStart > 0 && !blockText.at(tokenStart - 1).isSpace()) {
        --tokenStart;
    }
    while (tokenEnd < blockText.length() && !blockText.at(tokenEnd).isSpace()) {
        ++tokenEnd;
    }
    const QString rawTokenAtCursor = blockText.mid(tokenStart, tokenEnd - tokenStart).trimmed();

    if (command == QStringLiteral("input") && cursorInsideToken) {
        const QString tokenPrefix = currentToken.trimmed();
        if (!tokenPrefix.isEmpty()) {
            effectivePrefix = tokenPrefix;
        }
    }
    QStringList candidates;
    bool allowGlobalFallback = true;
    bool inputFileContext = false;
    struct ActiveValueContext
    {
        bool active = false;
        QString optionToken;
        QString arity;
        int optionIndex = -1;
        int nextOptionIndex = -1;
    };
    ActiveValueContext activeValueContext;

    if (tokenIndexAtCursor <= 0 && !effectivePrefix.startsWith(QLatin1Char('-'))) {
        const QStringList scopeStack = activeCompletionScopeStack();
        const QString activeScope = scopeStack.isEmpty() ? QStringLiteral("none") : scopeStack.constLast();
        const bool strictScopedCommandContext = !scopeStack.isEmpty();

        if (scopeStack.isEmpty()) {
            appendUniqueList(candidates, owner_->contextCommandTokens_.value(QStringLiteral("none")));
            appendUniqueList(candidates, owner_->contextCommandTokens_.value(QStringLiteral("all")));
        } else {
            appendUniqueList(candidates, owner_->contextCommandTokens_.value(activeScope));
            appendUniqueList(candidates, owner_->contextCommandTokens_.value(QStringLiteral("all")));
        }

        if (candidates.isEmpty() && !strictScopedCommandContext) {
            candidates = owner_->commandCompletionTokens_;
        }
    } else if (!command.isEmpty()) {
        const bool optionContext = currentToken.startsWith(QLatin1Char('-'))
            || effectivePrefix.startsWith(QLatin1Char('-'))
            || (previousToken.startsWith(QLatin1Char('-')) && effectivePrefix.startsWith(QLatin1Char('-')));
        if (!optionContext) {
            const QString normalizedPreviousToken = previousToken.trimmed().toLower();
            if (normalizedPreviousToken.startsWith(QLatin1Char('-'))) {
                activeValueContext.active = true;
                activeValueContext.optionToken = normalizedPreviousToken;
                activeValueContext.arity = owner_->commandOptionValueArityTokens_
                    .value(commandOptionValueKey(command, activeValueContext.optionToken))
                    .trimmed()
                    .toUpper();
            } else {
                const int scanTokenIndex = cursorInsideToken ? tokenIndexAtCursor : tokenIndexAtCursor - 1;
                if (scanTokenIndex >= 1 && scanTokenIndex < parsedLine.tokens.size()) {
                    int optionIndex = -1;
                    for (int index = scanTokenIndex; index >= 1; --index) {
                        const QString token = parsedLine.tokens.at(index).trimmed().toLower();
                        if (token.startsWith(QLatin1Char('-'))) {
                            optionIndex = index;
                            break;
                        }
                    }

                    if (optionIndex >= 1) {
                        int nextOptionIndex = parsedLine.tokens.size();
                        for (int index = optionIndex + 1; index < parsedLine.tokens.size(); ++index) {
                            const QString token = parsedLine.tokens.at(index).trimmed();
                            if (token.startsWith(QLatin1Char('-'))) {
                                nextOptionIndex = index;
                                break;
                            }
                        }

                        const QString optionToken = parsedLine.tokens.at(optionIndex).trimmed().toLower();
                        const QString arity = owner_->commandOptionValueArityTokens_
                            .value(commandOptionValueKey(command, optionToken))
                            .trimmed()
                            .toUpper();

                        const bool cursorWithinValueRange = tokenIndexAtCursor > optionIndex
                            && tokenIndexAtCursor <= nextOptionIndex
                            && !(cursorInsideToken && currentToken.trimmed().startsWith(QLatin1Char('-')));
                        if (cursorWithinValueRange) {
                            const int valueOrdinal = tokenIndexAtCursor - optionIndex;
                            const QString normalizedArity = canonicalOptionArityToken(arity);
                            const bool multiValueOption = normalizedArity == QStringLiteral("ONE_OR_MORE")
                                || normalizedArity == QStringLiteral("ZERO_OR_MORE");
                            const bool singleValueOption = normalizedArity == QStringLiteral("EXACTLY_ONE");
                            if (multiValueOption || (singleValueOption && valueOrdinal == 1)) {
                                activeValueContext.active = true;
                                activeValueContext.optionToken = optionToken;
                                activeValueContext.arity = arity;
                                activeValueContext.optionIndex = optionIndex;
                                activeValueContext.nextOptionIndex = nextOptionIndex;
                            }
                        }
                    }
                }
            }
        }
        const bool valueContext = activeValueContext.active;
        inputFileContext = command == QStringLiteral("input") && !optionContext;

        if (inputFileContext) {
            allowGlobalFallback = false;
            if (isActivatedInputPathPrefix(rawTokenAtCursor)) {
                candidates = projectInputFileCompletionCandidates();
            }
        } else if (optionContext) {
            allowGlobalFallback = false;
            candidates = owner_->commandOptionTokens_.value(command);
            QSet<QString> usedOptionTokens;
            for (const QString &lineToken : parsedLine.tokens) {
                const QString normalizedLineToken = lineToken.trimmed().toLower();
                if (normalizedLineToken.startsWith(QLatin1Char('-'))) {
                    usedOptionTokens.insert(normalizedLineToken);
                }
            }

            const QString activeOptionToken = cursorInsideToken && currentToken.trimmed().startsWith(QLatin1Char('-'))
                ? currentToken.trimmed().toLower()
                : QString();
            QStringList optionCandidates;
            for (const QString &candidate : candidates) {
                if (!candidate.startsWith(QLatin1Char('-'))) {
                    continue;
                }

                const QString normalizedCandidate = candidate.trimmed().toLower();
                if (usedOptionTokens.contains(normalizedCandidate) && normalizedCandidate != activeOptionToken) {
                    continue;
                }

                if (candidate.startsWith(QLatin1Char('-'))) {
                    appendUnique(optionCandidates, candidate);
                }
            }
            candidates = optionCandidates;
        } else if (valueContext) {
            allowGlobalFallback = false;
            const QString valueOptionToken = activeValueContext.optionToken;
            if (valueOptionToken == QStringLiteral("-subtype")) {
                const QString symbolTypeToken = symbolTypeForSubtypeLookup(command, parsedLine);
                const QHash<QString, QStringList> subtypeByType = owner_->commandSubtypeByTypeTokens_.value(command);

                if (!symbolTypeToken.isEmpty()) {
                    appendConcreteSubtypeValues(candidates, subtypeByType.value(symbolTypeToken));
                }

                if (candidates.isEmpty()) {
                    for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
                        appendConcreteSubtypeValues(candidates, subtypeIterator.value());
                    }
                }
            }

            if (candidates.isEmpty()) {
                candidates = owner_->commandOptionValueTokens_.value(commandOptionValueKey(command, valueOptionToken));
            }

            if (canonicalOptionArityToken(activeValueContext.arity) == QStringLiteral("ONE_OR_MORE")
                && activeValueContext.optionIndex >= 0
                && !candidates.isEmpty()) {
                QSet<QString> usedValues;
                for (int index = activeValueContext.optionIndex + 1; index < activeValueContext.nextOptionIndex; ++index) {
                    if (cursorInsideToken && index == tokenIndexAtCursor) {
                        continue;
                    }
                    usedValues.insert(parsedLine.tokens.at(index).trimmed().toLower());
                }

                QStringList filteredCandidates;
                for (const QString &candidate : candidates) {
                    if (!usedValues.contains(candidate.trimmed().toLower())) {
                        appendUnique(filteredCandidates, candidate);
                    }
                }
                candidates = filteredCandidates;
            }
        } else {
            const int requiredPositionalCount = qMax(0, owner_->commandRequiredPositionalCount_.value(command));
            const int positionalCountBeforeCursor = positionalTokenCountBeforeCursor(parsedLine, tokenIndexAtCursor);
            if (requiredPositionalCount > 0 && positionalCountBeforeCursor < requiredPositionalCount) {
                allowGlobalFallback = false;
                candidates = owner_->commandValueTokens_.value(command);
            } else {
                candidates = owner_->commandValueTokens_.value(command);
                appendUniqueList(candidates, owner_->commandOptionTokens_.value(command));
            }
        }
    }

    if (candidates.isEmpty() && allowGlobalFallback) {
        candidates = owner_->completionTokens_;
    }

    QStringList filtered;
    QString inputPathPrefix;
    QString normalizedInputPathPrefix;
    if (inputFileContext) {
        const QString rawInputPrefix = rawTokenAtCursor;
        inputPathPrefix = QDir::fromNativeSeparators(rawInputPrefix);
        normalizedInputPathPrefix = normalizeInputCompletionPrefix(rawInputPrefix);
    }
    const bool sameDirectoryHint = inputFileContext && inputPathPrefix.startsWith(QStringLiteral("./"));
    const bool treatAsEmptyInputPrefix = inputFileContext
        && !inputPathPrefix.isEmpty()
        && normalizedInputPathPrefix.isEmpty();
    for (const QString &candidate : candidates) {
        if (inputFileContext && sameDirectoryHint && candidate.startsWith(QStringLiteral("../"))) {
            continue;
        }

        const bool matchesDefaultPrefix = treatAsEmptyInputPrefix
            || effectivePrefix.isEmpty()
            || candidate.startsWith(effectivePrefix, Qt::CaseInsensitive);
        const bool matchesNormalizedInputPrefix = inputFileContext
            && !normalizedInputPathPrefix.isEmpty()
            && candidate.startsWith(normalizedInputPathPrefix, Qt::CaseInsensitive);
        if (matchesDefaultPrefix || matchesNormalizedInputPrefix) {
            appendUnique(filtered, candidate);
        }
    }

    std::sort(filtered.begin(),
              filtered.end(),
              [](const QString &a, const QString &b) {
                  return QString::compare(a, b, Qt::CaseInsensitive) < 0;
              });

    return filtered;
}

void RawEditorCompletionController::triggerCompletionPopup()
{
    if (owner_ == nullptr
        || owner_->editor_ == nullptr
        || owner_->completionCompleter_ == nullptr
        || owner_->completionModel_ == nullptr) {
        return;
    }

    const QString command = currentCompletionCommand();

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

    const QString prefix = currentCompletionPrefix();
    QStringList suggestions = buildCompletionSuggestionsForCursor(prefix);
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

        const int requiredPositionalCount = qMax(0, owner_->commandRequiredPositionalCount_.value(command));
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
        popupPrefix = normalizeInputCompletionPrefix(rawPathPrefix);
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

    const QString scopeLabel = currentCompletionScopeLabel();
    if (!scopeLabel.isEmpty()) {
        QToolTip::showText(owner_->editor_->mapToGlobal(cursorRect.bottomLeft()),
                           owner_->tr("Completion scope: %1").arg(scopeLabel),
                           owner_->editor_,
                           QRect(),
                           1500);
    }
}

void RawEditorCompletionController::insertCompletionToken(const QString &completion)
{
    if (owner_ == nullptr || owner_->editor_ == nullptr || completion.trimmed().isEmpty()) {
        return;
    }

    QTextCursor cursor = owner_->editor_->textCursor();
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

    const QString normalizedCompletion = owner_->normalizedDirectiveToken(completion.toLower());
    const QString closingDirective = owner_->closingDirectiveForOpeningToken(normalizedCompletion);
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
            if (owner_->normalizedDirectiveToken(nextText) == closingDirective) {
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
    owner_->editor_->setTextCursor(cursor);
}
}
