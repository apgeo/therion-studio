#include "RawEditorCompletionContextAnalyzer.h"

#include "../TextEditorCommandMetadata.h"
#include "../TextEditorTab.h"

#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>

#include "../../../core/TherionDocumentParser.h"

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
}

namespace TherionStudio
{
RawEditorCompletionContextAnalyzer::RawEditorCompletionContextAnalyzer(TextEditorTab *owner)
    : owner_(owner)
{
}

const TextEditorCommandMetadata &RawEditorCompletionContextAnalyzer::metadata() const
{
    return owner_->commandMetadata();
}

QString RawEditorCompletionContextAnalyzer::currentCompletionPrefix() const
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

QStringList RawEditorCompletionContextAnalyzer::activeCompletionScopeStack() const
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

QString RawEditorCompletionContextAnalyzer::currentCompletionCommand() const
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
    if (metadata().commandCompletionTokens.contains(directive, Qt::CaseInsensitive)
        || metadata().commandOptionTokens.contains(directive)
        || metadata().commandValueTokens.contains(directive)) {
        return directive;
    }

    const QStringList scopeStack = activeCompletionScopeStack();
    for (int index = scopeStack.size() - 1; index >= 0; --index) {
        const QString scopeDirective = scopeStack.at(index);
        if (metadata().commandCompletionTokens.contains(scopeDirective, Qt::CaseInsensitive)) {
            return scopeDirective;
        }
    }
    return QString();
}

QString RawEditorCompletionContextAnalyzer::currentCompletionScopeLabel() const
{
    const QStringList scopeStack = activeCompletionScopeStack();
    if (scopeStack.isEmpty()) {
        return owner_ != nullptr ? owner_->tr("top-level") : QStringLiteral("top-level");
    }

    return scopeStack.constLast();
}

QString RawEditorCompletionContextAnalyzer::normalizeCompletionContext(const QString &contextToken) const
{
    return normalizedCompletionContextToken(contextToken);
}

bool RawEditorCompletionContextAnalyzer::isCompatibleChildKindForBlocks(const QString &parentKind,
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
    const QStringList childContexts = metadata().blockCommandContextsByKind.value(normalizedChild);
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

bool RawEditorCompletionContextAnalyzer::isCommandDirectiveInScope(const QString &directive,
                                                                   const QString &scopeToken) const
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

    QStringList candidates = metadata().contextCommandTokens.value(normalizedScope);
    appendUniqueList(candidates, metadata().contextCommandTokens.value(QStringLiteral("all")));
    if (normalizedScope == QStringLiteral("none")) {
        appendUniqueList(candidates, metadata().contextCommandTokens.value(QStringLiteral("none")));
    }
    return candidates.contains(normalizedDirective, Qt::CaseInsensitive);
}

QString RawEditorCompletionContextAnalyzer::primaryInsertionScopeForCommand(const QString &commandToken) const
{
    if (owner_ == nullptr) {
        return QString();
    }

    const QString normalizedCommand = owner_->normalizedDirectiveToken(commandToken.trimmed());
    const QStringList contexts = metadata().blockCommandContextsByKind.value(normalizedCommand);
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

QString RawEditorCompletionContextAnalyzer::resolveScopeForCommandAtLine(const QString &commandToken,
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
}
