#include "RawEditorCompletionController.h"

#include "RawEditorCompletionContextAnalyzer.h"
#include "RawEditorCommandMetadataLoader.h"
#include "../TextEditorTab.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPlainTextEdit>
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
            appendUniqueList(candidates, mutableCommandMetadata().contextCommandTokens.value(QStringLiteral("none")));
            appendUniqueList(candidates, mutableCommandMetadata().contextCommandTokens.value(QStringLiteral("all")));
        } else {
            appendUniqueList(candidates, mutableCommandMetadata().contextCommandTokens.value(activeScope));
            appendUniqueList(candidates, mutableCommandMetadata().contextCommandTokens.value(QStringLiteral("all")));
        }

        if (candidates.isEmpty() && !strictScopedCommandContext) {
            candidates = mutableCommandMetadata().commandCompletionTokens;
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
                activeValueContext.arity = mutableCommandMetadata().commandOptionValueArityTokens
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
                        const QString arity = mutableCommandMetadata().commandOptionValueArityTokens
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
            candidates = mutableCommandMetadata().commandOptionTokens.value(command);
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
                const QHash<QString, QStringList> subtypeByType = mutableCommandMetadata().commandSubtypeByTypeTokens.value(command);

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
                candidates = mutableCommandMetadata().commandOptionValueTokens.value(commandOptionValueKey(command, valueOptionToken));
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
            const int requiredPositionalCount = qMax(0, mutableCommandMetadata().commandRequiredPositionalCount.value(command));
            const int positionalCountBeforeCursor = positionalTokenCountBeforeCursor(parsedLine, tokenIndexAtCursor);
            if (requiredPositionalCount > 0 && positionalCountBeforeCursor < requiredPositionalCount) {
                allowGlobalFallback = false;
                candidates = mutableCommandMetadata().commandValueTokens.value(command);
            } else {
                candidates = mutableCommandMetadata().commandValueTokens.value(command);
                appendUniqueList(candidates, mutableCommandMetadata().commandOptionTokens.value(command));
            }
        }
    }

    if (candidates.isEmpty() && allowGlobalFallback) {
        candidates = mutableCommandMetadata().completionTokens;
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

        const int requiredPositionalCount = qMax(0, mutableCommandMetadata().commandRequiredPositionalCount.value(command));
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
