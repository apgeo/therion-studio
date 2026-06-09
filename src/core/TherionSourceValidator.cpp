#include "TherionSourceValidator.h"

#include "TherionCommandLineModel.h"
#include "TherionCommandSyntax.h"
#include "TherionDocumentParser.h"
#include "TherionSourceDocument.h"

#include <algorithm>

namespace TherionStudio
{
namespace
{
bool isPlainOptionToken(const QString &token)
{
    return commandTokenStartsNewOption(token)
        && !commandTokenEmbedsOptionValue(token);
}

bool tokenActsAsOptionBoundary(const TherionParsedLine &parsedLine, int tokenIndex)
{
    if (tokenIndex < 0 || tokenIndex >= parsedLine.tokens.size()) {
        return false;
    }

    const QString token = parsedLine.tokens.at(tokenIndex);
    if (!commandTokenStartsNewOption(token)) {
        return false;
    }

    if (commandTokenEmbedsOptionValue(token)
        && tokenIndex > 0
        && isPlainOptionToken(parsedLine.tokens.at(tokenIndex - 1))) {
        return false;
    }

    return true;
}

QPair<int, int> sourceLineRemovalRangeForTokenRange(const QString &lineText,
                                                    const TherionParsedLine &parsedLine,
                                                    int firstTokenIndex,
                                                    int lastTokenIndex)
{
    if (firstTokenIndex < 0
        || lastTokenIndex < firstTokenIndex
        || firstTokenIndex >= parsedLine.tokenSpans.size()
        || lastTokenIndex >= parsedLine.tokenSpans.size()) {
        return qMakePair(-1, -1);
    }

    const TherionParsedToken firstToken = parsedLine.tokenSpans.at(firstTokenIndex);
    const TherionParsedToken lastToken = parsedLine.tokenSpans.at(lastTokenIndex);
    int removeStart = firstToken.start;
    int removeEnd = lastToken.start + lastToken.length;
    if (removeStart < 0 || removeEnd < removeStart || removeEnd > lineText.size()) {
        return qMakePair(-1, -1);
    }

    if (removeStart > 0 && lineText.at(removeStart - 1).isSpace()) {
        --removeStart;
    } else if (removeEnd < lineText.size() && lineText.at(removeEnd).isSpace()) {
        ++removeEnd;
    }

    return qMakePair(removeStart, removeEnd);
}

QString lineWithRemovedTokenRange(const QString &lineText,
                                  const TherionParsedLine &parsedLine,
                                  int firstTokenIndex,
                                  int lastTokenIndex,
                                  bool *ok)
{
    if (ok != nullptr) {
        *ok = false;
    }

    const QPair<int, int> range =
        sourceLineRemovalRangeForTokenRange(lineText, parsedLine, firstTokenIndex, lastTokenIndex);
    if (range.first < 0 || range.second < range.first) {
        return lineText;
    }

    QString updated = lineText;
    updated.remove(range.first, range.second - range.first);
    if (ok != nullptr) {
        *ok = true;
    }
    return updated;
}

int optionValueEndTokenIndex(const TherionParsedLine &parsedLine, int optionTokenIndex)
{
    if (optionTokenIndex < 0 || optionTokenIndex >= parsedLine.tokens.size()) {
        return optionTokenIndex;
    }

    if (commandTokenEmbedsOptionValue(parsedLine.tokens.at(optionTokenIndex))) {
        return optionTokenIndex;
    }

    int valueEndTokenIndex = optionTokenIndex;
    for (int scan = optionTokenIndex + 1; scan < parsedLine.tokens.size(); ++scan) {
        if (tokenActsAsOptionBoundary(parsedLine, scan)) {
            break;
        }
        valueEndTokenIndex = scan;
    }

    return valueEndTokenIndex;
}

QStringList optionValueTokens(const TherionParsedLine &parsedLine, int optionTokenIndex)
{
    if (optionTokenIndex < 0 || optionTokenIndex >= parsedLine.tokens.size()) {
        return {};
    }

    const QString optionToken = parsedLine.tokens.at(optionTokenIndex);
    if (commandTokenEmbedsOptionValue(optionToken)) {
        return {commandEmbeddedOptionValue(optionToken)};
    }

    const int valueEndTokenIndex = optionValueEndTokenIndex(parsedLine, optionTokenIndex);
    if (valueEndTokenIndex <= optionTokenIndex) {
        return {};
    }

    return parsedLine.tokens.mid(optionTokenIndex + 1, valueEndTokenIndex - optionTokenIndex);
}

int logicalOptionValueCount(const QStringList &tokens)
{
    int count = 0;
    for (int index = 0; index < tokens.size(); ++index) {
        const QString token = tokens.at(index).trimmed();
        if (token == QStringLiteral("\\")) {
            continue;
        }
        if (token.startsWith(QLatin1Char('['))) {
            ++count;
            while (index + 1 < tokens.size()
                   && !tokens.at(index).contains(QLatin1Char(']'))) {
                ++index;
            }
            continue;
        }
        ++count;
    }
    return count;
}

QString optionNameToken(const QString &token)
{
    return commandTokenEmbedsOptionValue(token)
        ? commandEmbeddedOptionName(token)
        : token.trimmed();
}

bool looksLikeCommandDirective(const QString &token)
{
    const QString trimmed = token.trimmed();
    if (trimmed.isEmpty() || !trimmed.at(0).isLetter()) {
        return false;
    }
    for (const QChar character : trimmed) {
        if (character.isLetterOrNumber()
            || character == QLatin1Char('-')
            || character == QLatin1Char('_')) {
            continue;
        }
        return false;
    }
    return true;
}

QString optionDeduplicationKey(const QString &optionToken, const QStringList &values)
{
    return normalizedCommandOptionName(optionToken)
        + QLatin1Char('\n')
        + values.join(QLatin1Char('\n'));
}

QString cleanupInvalidSubtypeValues(const QString &lineText, bool *changed)
{
    if (changed != nullptr) {
        *changed = false;
    }

    QString updated = lineText;
    for (;;) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(updated);
        int removeStartTokenIndex = -1;
        int removeEndTokenIndex = -1;
        for (int index = 1; index < parsedLine.tokens.size(); ++index) {
            if (!tokenActsAsOptionBoundary(parsedLine, index)) {
                continue;
            }

            const QString optionToken = optionNameToken(parsedLine.tokens.at(index)).toLower();
            if (optionToken != QStringLiteral("-subtype")) {
                continue;
            }

            const int valueEndTokenIndex = optionValueEndTokenIndex(parsedLine, index);
            for (int valueIndex = index + 1; valueIndex <= valueEndTokenIndex; ++valueIndex) {
                if (valueIndex >= parsedLine.tokens.size()) {
                    break;
                }
                if (commandTokenEmbedsOptionValue(parsedLine.tokens.at(valueIndex))) {
                    removeStartTokenIndex = index;
                    removeEndTokenIndex = valueEndTokenIndex;
                    break;
                }
            }
            if (removeStartTokenIndex >= 0) {
                break;
            }
        }

        if (removeStartTokenIndex < 0) {
            return updated;
        }

        bool removed = false;
        updated = lineWithRemovedTokenRange(updated,
                                            parsedLine,
                                            removeStartTokenIndex,
                                            removeEndTokenIndex,
                                            &removed);
        if (!removed) {
            return updated;
        }
        if (changed != nullptr) {
            *changed = true;
        }
    }
}

QString cleanupDuplicateOptions(const QString &lineText, bool *changed)
{
    if (changed != nullptr) {
        *changed = false;
    }

    QString updated = lineText;
    for (;;) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(updated);
        QStringList seenOptions;
        int removeStartTokenIndex = -1;
        int removeEndTokenIndex = -1;

        for (int index = 1; index < parsedLine.tokens.size(); ++index) {
            if (!tokenActsAsOptionBoundary(parsedLine, index)) {
                continue;
            }

            const QString optionToken = optionNameToken(parsedLine.tokens.at(index));
            const QStringList values = optionValueTokens(parsedLine, index);
            const QString deduplicationKey = optionDeduplicationKey(optionToken, values);
            if (seenOptions.contains(deduplicationKey)) {
                removeStartTokenIndex = index;
                removeEndTokenIndex = optionValueEndTokenIndex(parsedLine, index);
                break;
            }

            seenOptions.append(deduplicationKey);
            index = optionValueEndTokenIndex(parsedLine, index);
        }

        if (removeStartTokenIndex < 0) {
            return updated;
        }

        bool removed = false;
        updated = lineWithRemovedTokenRange(updated,
                                            parsedLine,
                                            removeStartTokenIndex,
                                            removeEndTokenIndex,
                                            &removed);
        if (!removed) {
            return updated;
        }
        if (changed != nullptr) {
            *changed = true;
        }
    }
}

QString cleanupLine(const QString &lineText, bool *changed)
{
    bool subtypeChanged = false;
    QString updated = cleanupInvalidSubtypeValues(lineText, &subtypeChanged);

    bool duplicateChanged = false;
    updated = cleanupDuplicateOptions(updated, &duplicateChanged);

    if (changed != nullptr) {
        *changed = subtypeChanged || duplicateChanged;
    }
    return updated;
}

TherionSourceDiagnostic diagnosticForLineCleanup(const TherionParsedSourceLine &sourceLine,
                                                 const QString &suggestedText)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("malformed-option-token");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = sourceLine.lineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.title = QStringLiteral("Malformed or duplicate option token");
    diagnostic.message = QStringLiteral("This command contains an option-like token or duplicate option/value pair that Therion may reject.");
    diagnostic.currentText = sourceLine.text;
    diagnostic.suggestedText = suggestedText;
    diagnostic.hasFix = true;
    diagnostic.fix.startOffset = sourceLine.startOffset;
    diagnostic.fix.length = sourceLine.textLength;
    diagnostic.fix.replacementText = suggestedText;
    diagnostic.fix.description = QStringLiteral("Rewrite line %1").arg(sourceLine.lineNumber);
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForLine(const TherionParsedSourceLine &sourceLine,
                                          const QString &code,
                                          const QString &title,
                                          const QString &message,
                                          TherionSourceDiagnosticSeverity severity = TherionSourceDiagnosticSeverity::Warning)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = severity;
    diagnostic.lineNumber = sourceLine.lineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.title = title;
    diagnostic.message = message;
    diagnostic.currentText = sourceLine.text;
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = false;
    return diagnostic;
}

int firstOptionTokenIndex(const TherionParsedLine &parsedLine)
{
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (tokenActsAsOptionBoundary(parsedLine, index)) {
            return index;
        }
    }
    return parsedLine.tokens.size();
}

int positionalTokenCount(const TherionParsedLine &parsedLine)
{
    const int optionStart = firstOptionTokenIndex(parsedLine);
    return qMax(0, optionStart - 1);
}

void appendCommandCatalogDiagnostics(TherionSourceValidationResult *result,
                                     const TherionParsedSourceLine &sourceLine,
                                     const TherionSourceValidationCatalog &catalog,
                                     bool skipUnknownCommand)
{
    if (result == nullptr || sourceLine.parsed.tokens.isEmpty()) {
        return;
    }

    const QString commandName = normalizedTherionDirectiveToken(sourceLine.parsed.directive);
    const bool commandKnown = catalog.commandNames.contains(commandName);
    if (!commandKnown) {
        if (!skipUnknownCommand
            && !therionDirectiveIsKnownBlockDirective(commandName)
            && looksLikeCommandDirective(sourceLine.parsed.directive)) {
            result->diagnostics.append(diagnosticForLine(sourceLine,
                                                         QStringLiteral("unknown-command"),
                                                         QStringLiteral("Unknown command"),
                                                         QStringLiteral("Command `%1` is not present in the Therion command catalog.").arg(sourceLine.parsed.directive)));
        }
        return;
    }

    const int requiredPositionalCount = qMax(0, catalog.commandRequiredPositionalCount.value(commandName, 0));
    const int providedPositionalCount = positionalTokenCount(sourceLine.parsed);
    if (requiredPositionalCount > 0 && providedPositionalCount < requiredPositionalCount) {
        result->diagnostics.append(diagnosticForLine(sourceLine,
                                                     QStringLiteral("missing-argument"),
                                                     QStringLiteral("Missing argument"),
                                                     QStringLiteral("Command `%1` expects at least %2 positional argument(s), but %3 provided.")
                                                         .arg(commandName)
                                                         .arg(requiredPositionalCount)
                                                         .arg(providedPositionalCount),
                                                     TherionSourceDiagnosticSeverity::Error));
    }

    const QStringList allowedFirstValues =
        catalog.commandArgumentAllowedValuesByKey.value(commandArgumentValueKey(commandName, 0));
    if (!allowedFirstValues.isEmpty() && providedPositionalCount > 0 && sourceLine.parsed.tokens.size() > 1) {
        const QString value = sourceLine.parsed.tokens.at(1).trimmed();
        if (!value.isEmpty() && !allowedFirstValues.contains(value, Qt::CaseInsensitive)) {
            result->diagnostics.append(diagnosticForLine(sourceLine,
                                                         QStringLiteral("unknown-argument-value"),
                                                         QStringLiteral("Unknown argument value"),
                                                         QStringLiteral("Command `%1` does not list `%2` as a known value. Known values: %3.")
                                                             .arg(commandName, value, allowedFirstValues.join(QStringLiteral(", ")))));
        }
    }

    const QSet<QString> knownOptions = catalog.commandOptionNames.value(commandName);
    for (int index = 1; index < sourceLine.parsed.tokens.size(); ++index) {
        if (!tokenActsAsOptionBoundary(sourceLine.parsed, index)) {
            continue;
        }

        const QString optionToken = optionNameToken(sourceLine.parsed.tokens.at(index));
        const QString normalizedOption = QStringLiteral("-") + normalizedCommandOptionName(optionToken);
        if (!knownOptions.isEmpty() && !knownOptions.contains(normalizedOption)) {
            result->diagnostics.append(diagnosticForLine(sourceLine,
                                                         QStringLiteral("unknown-option"),
                                                         QStringLiteral("Unknown option"),
                                                         QStringLiteral("Command `%1` does not list option `%2` in the Therion command catalog.")
                                                             .arg(commandName, optionToken)));
        }

        const QString arity = catalog.commandOptionValueArityTokens.value(commandOptionValueKey(commandName, normalizedOption));
        const int fixedArity = catalog.commandOptionFixedArityByKey.value(commandOptionValueKey(commandName, normalizedOption), -1);
        const int nextOptionIndex = optionValueEndTokenIndex(sourceLine.parsed, index) + 1;
        const int providedValueCount = commandTokenEmbedsOptionValue(sourceLine.parsed.tokens.at(index))
            ? 1
            : logicalOptionValueCount(optionValueTokens(sourceLine.parsed, index));
        if ((optionArityRequiresValue(arity) || fixedArity > 0) && providedValueCount == 0) {
            result->diagnostics.append(diagnosticForLine(sourceLine,
                                                         QStringLiteral("missing-option-value"),
                                                         QStringLiteral("Missing option value"),
                                                         QStringLiteral("Option `%1` on command `%2` expects a value.")
                                                             .arg(optionToken, commandName),
                                                         TherionSourceDiagnosticSeverity::Error));
        } else if (fixedArity > 0 && providedValueCount > 0 && providedValueCount != fixedArity) {
            result->diagnostics.append(diagnosticForLine(sourceLine,
                                                         QStringLiteral("wrong-option-value-count"),
                                                         QStringLiteral("Unexpected option value count"),
                                                         QStringLiteral("Option `%1` on command `%2` expects exactly %3 value(s), but %4 provided.")
                                                             .arg(optionToken)
                                                             .arg(commandName)
                                                             .arg(fixedArity)
                                                             .arg(providedValueCount),
                                                         TherionSourceDiagnosticSeverity::Error));
        }

        index = qMax(index, nextOptionIndex - 1);
    }
}

void appendBlockDiagnostics(TherionSourceValidationResult *result,
                            const TherionSourceDocument &sourceDocument)
{
    if (result == nullptr) {
        return;
    }

    const QHash<QString, QString> openToClose = therionOpenToCloseDirectiveMap();
    for (const TherionSourceDocumentLine &line : sourceDocument.lines()) {
        if (!line.hasUnmatchedClose()) {
            continue;
        }
        result->diagnostics.append(diagnosticForLine(line.sourceLine,
                                                     QStringLiteral("unmatched-block-close"),
                                                     QStringLiteral("Unmatched block close"),
                                                     QStringLiteral("Closing directive `%1` does not match the currently open block.").arg(line.sourceLine.parsed.directive),
                                                     TherionSourceDiagnosticSeverity::Error));
    }

    for (const TherionSourceBlockFrame &openBlock : sourceDocument.openBlocksAtEnd()) {
        TherionSourceDiagnostic diagnostic;
        diagnostic.code = QStringLiteral("unclosed-block");
        diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
        diagnostic.lineNumber = openBlock.lineNumber;
        diagnostic.columnNumber = 1;
        diagnostic.title = QStringLiteral("Unclosed block");
        diagnostic.message = QStringLiteral("Block `%1` is not closed before the end of the document. Expected `%2`.")
                                 .arg(openBlock.directive, openToClose.value(openBlock.directive));
        diagnostic.currentText = openBlock.lineText;
        result->diagnostics.append(diagnostic);
    }
}
}

TherionSourceValidationResult TherionSourceValidator::validate(const QString &contents)
{
    return validate(contents, {});
}

TherionSourceValidationResult TherionSourceValidator::validate(const QString &contents,
                                                               const TherionSourceValidationCatalog &catalog)
{
    TherionSourceValidationResult result;
    const TherionSourceDocument sourceDocument = TherionSourceDocument::fromText(contents);
    for (const TherionSourceDocumentLine &documentLine : sourceDocument.lines()) {
        const TherionParsedSourceLine &sourceLine = documentLine.sourceLine;

        bool changed = false;
        const QString suggestedText = cleanupLine(sourceLine.text, &changed);
        if (changed && suggestedText != sourceLine.text) {
            result.diagnostics.append(diagnosticForLineCleanup(sourceLine, suggestedText));
        }

        if (!catalog.commandNames.isEmpty() && documentLine.shouldValidateCommandCatalog()) {
            appendCommandCatalogDiagnostics(&result,
                                            sourceLine,
                                            catalog,
                                            false);
        }
    }

    if (!catalog.commandNames.isEmpty()) {
        appendBlockDiagnostics(&result, sourceDocument);
    }
    return result;
}

QString TherionSourceValidator::applyFixes(const QString &contents,
                                           const QVector<TherionSourceDiagnosticFix> &fixes)
{
    QVector<TherionSourceDiagnosticFix> sortedFixes = fixes;
    std::sort(sortedFixes.begin(),
              sortedFixes.end(),
              [](const TherionSourceDiagnosticFix &left, const TherionSourceDiagnosticFix &right) {
                  return left.startOffset > right.startOffset;
              });

    QString updated = contents;
    for (const TherionSourceDiagnosticFix &fix : std::as_const(sortedFixes)) {
        if (fix.startOffset < 0
            || fix.length < 0
            || fix.startOffset + fix.length > updated.size()) {
            continue;
        }
        updated.replace(fix.startOffset, fix.length, fix.replacementText);
    }
    return updated;
}
}
