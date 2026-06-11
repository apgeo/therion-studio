#include "RawEditorCompletionContextAnalyzer.h"

#include "../TextEditorCommandMetadata.h"

#include <QCoreApplication>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>

#include "../../../core/TherionSourceLogicalDocument.h"

#include <utility>

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

TherionStudio::TherionSourceLogicalDocument logicalDocumentForEditor(const QPlainTextEdit *editor)
{
    return editor == nullptr
        ? TherionStudio::TherionSourceLogicalDocument::fromText(QString())
        : TherionStudio::TherionSourceLogicalDocument::fromText(editor->toPlainText());
}

void removeMatchingOpenDirective(QStringList *scopeStack, const QString &openingDirective)
{
    if (scopeStack == nullptr || openingDirective.isEmpty()) {
        return;
    }

    for (int stackIndex = scopeStack->size() - 1; stackIndex >= 0; --stackIndex) {
        if (scopeStack->at(stackIndex) == openingDirective) {
            scopeStack->removeAt(stackIndex);
            break;
        }
    }
}

QStringList scopeStackBeforeLine(const QVector<TherionStudio::TherionSourceLogicalCommand> &commands,
                                 int oneBasedLineNumber,
                                 const std::function<QString(const QString &)> &normalizedDirectiveToken,
                                 const std::function<QString(const QString &)> &openingDirectiveForClosingToken,
                                 const std::function<bool(const QString &, const TherionStudio::TherionParsedLine &)> &isContainerDirectiveInstance)
{
    QStringList scopeStack;
    if (!normalizedDirectiveToken || !openingDirectiveForClosingToken || !isContainerDirectiveInstance) {
        return scopeStack;
    }

    for (const TherionStudio::TherionSourceLogicalCommand &command : commands) {
        if (command.startLineNumber >= oneBasedLineNumber) {
            break;
        }
        if (command.parsed.directive.isEmpty()) {
            continue;
        }

        const QString directive = normalizedDirectiveToken(command.parsed.directive);
        const QString openingFromClosing = openingDirectiveForClosingToken(directive);
        if (!openingFromClosing.isEmpty()) {
            removeMatchingOpenDirective(&scopeStack, openingFromClosing);
            continue;
        }

        if (isContainerDirectiveInstance(directive, command.parsed)) {
            scopeStack.append(directive);
        }
    }

    return scopeStack;
}
}

namespace TherionStudio
{
RawEditorCompletionContextAnalyzer::RawEditorCompletionContextAnalyzer(RawEditorCompletionContext context)
    : context_(std::move(context))
{
}

const TextEditorCommandMetadata &RawEditorCompletionContextAnalyzer::metadata() const
{
    return *context_.metadata;
}

QString RawEditorCompletionContextAnalyzer::currentCompletionPrefix() const
{
    if (context_.editor == nullptr) {
        return QString();
    }

    const QTextCursor cursor = context_.editor->textCursor();
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
    if (context_.editor == nullptr || !context_.normalizedDirectiveToken || !context_.openingDirectiveForClosingToken
        || !context_.isContainerDirectiveInstance) {
        return scopeStack;
    }

    const QTextCursor cursor = context_.editor->textCursor();
    const int currentBlockNumber = cursor.block().blockNumber();
    if (currentBlockNumber <= 0) {
        return scopeStack;
    }

    const TherionSourceLogicalDocument logicalDocument = logicalDocumentForEditor(context_.editor);
    return scopeStackBeforeLine(logicalDocument.commands(),
                                currentBlockNumber + 1,
                                context_.normalizedDirectiveToken,
                                context_.openingDirectiveForClosingToken,
                                context_.isContainerDirectiveInstance);
}

QString RawEditorCompletionContextAnalyzer::currentCompletionCommand() const
{
    if (context_.editor == nullptr || context_.metadata == nullptr || !context_.normalizedDirectiveToken) {
        return QString();
    }

    const QTextCursor cursor = context_.editor->textCursor();
    const QTextBlock block = cursor.block();
    if (!block.isValid()) {
        return QString();
    }

    const TherionSourceLogicalDocument logicalDocument = logicalDocumentForEditor(context_.editor);
    const TherionSourceLogicalCommand *command = logicalDocument.commandAtPhysicalLine(block.blockNumber() + 1);
    if (command == nullptr || command->parsed.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = context_.normalizedDirectiveToken(command->parsed.directive.toLower());
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
        return QCoreApplication::translate("TherionStudio::TextEditorTab", "top-level");
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
    if (context_.metadata == nullptr || !context_.normalizedDirectiveToken) {
        return false;
    }

    const QString normalizedParent = context_.normalizedDirectiveToken(parentKind.trimmed().toLower());
    const QString normalizedChild = context_.normalizedDirectiveToken(childKind.trimmed().toLower());
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
    if (context_.metadata == nullptr || !context_.normalizedDirectiveToken) {
        return false;
    }

    const QString normalizedDirective = context_.normalizedDirectiveToken(directive.trimmed());
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
    if (context_.metadata == nullptr || !context_.normalizedDirectiveToken) {
        return QString();
    }

    const QString normalizedCommand = context_.normalizedDirectiveToken(commandToken.trimmed());
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
    if (context_.metadata == nullptr || !context_.normalizedDirectiveToken || !context_.openingDirectiveForClosingToken
        || !context_.isContainerDirectiveInstance) {
        return QString();
    }

    const QString normalizedCommand = context_.normalizedDirectiveToken(commandToken.trimmed());
    if (normalizedCommand.isEmpty()) {
        return QString();
    }

    const QString preferredScope = primaryInsertionScopeForCommand(normalizedCommand);
    if (!preferredScope.isEmpty()) {
        return preferredScope;
    }

    const TherionSourceLogicalDocument logicalDocument = TherionSourceLogicalDocument::fromText(lines.join(QLatin1Char('\n')));
    const QStringList scopeStack = scopeStackBeforeLine(logicalDocument.commands(),
                                                        lineNumber,
                                                        context_.normalizedDirectiveToken,
                                                        context_.openingDirectiveForClosingToken,
                                                        context_.isContainerDirectiveInstance);

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
