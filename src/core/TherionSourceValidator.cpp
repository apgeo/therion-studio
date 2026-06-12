#include "TherionSourceValidator.h"

#include "TherionCommandLineModel.h"
#include "TherionCommandSyntax.h"
#include "TherionDocumentEditor.h"
#include "TherionDocumentParser.h"
#include "TherionSourceDocument.h"
#include "TherionSourceLogicalDocument.h"

#include <algorithm>
#include <optional>

namespace TherionStudio
{
namespace
{
struct LineCleanupResult
{
    QString text;
    bool changed = false;
    int columnNumber = 1;
    int columnLength = 0;
};

std::optional<TherionParsedToken> tokenSpanForCommandTokenIndex(const TherionParsedLine &parsedLine,
                                                                int tokenIndex)
{
    if (tokenIndex < 0) {
        return std::nullopt;
    }

    int currentTokenIndex = 0;
    for (const TherionParsedToken &tokenSpan : parsedLine.tokenSpans) {
        if (tokenSpan.type == TherionTokenType::Comment) {
            continue;
        }
        if (currentTokenIndex == tokenIndex) {
            return tokenSpan;
        }
        ++currentTokenIndex;
    }
    return std::nullopt;
}

QPair<int, int> sourceLineRemovalRangeForTokenRange(const QString &lineText,
                                                    const TherionParsedLine &parsedLine,
                                                    int firstTokenIndex,
                                                    int lastTokenIndex)
{
    if (firstTokenIndex < 0
        || lastTokenIndex < firstTokenIndex) {
        return qMakePair(-1, -1);
    }

    const std::optional<TherionParsedToken> firstToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                       firstTokenIndex);
    const std::optional<TherionParsedToken> lastToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                     lastTokenIndex);
    if (!firstToken.has_value() || !lastToken.has_value()) {
        return qMakePair(-1, -1);
    }

    int removeStart = firstToken->start;
    int removeEnd = lastToken->start + lastToken->length;
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

bool setRangeFromTokenRange(const QString &lineText,
                            const TherionParsedLine &parsedLine,
                            int firstTokenIndex,
                            int lastTokenIndex,
                            int *columnNumber,
                            int *columnLength)
{
    if (columnNumber != nullptr) {
        *columnNumber = 1;
    }
    if (columnLength != nullptr) {
        *columnLength = 0;
    }

    if (firstTokenIndex < 0
        || lastTokenIndex < firstTokenIndex) {
        return false;
    }

    const std::optional<TherionParsedToken> firstToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                       firstTokenIndex);
    const std::optional<TherionParsedToken> lastToken = tokenSpanForCommandTokenIndex(parsedLine,
                                                                                     lastTokenIndex);
    if (!firstToken.has_value() || !lastToken.has_value()) {
        return false;
    }

    const int rangeStart = firstToken->start;
    const int rangeEnd = lastToken->start + lastToken->length;
    if (rangeStart < 0 || rangeEnd <= rangeStart || rangeEnd > lineText.size()) {
        return false;
    }

    if (columnNumber != nullptr) {
        *columnNumber = rangeStart + 1;
    }
    if (columnLength != nullptr) {
        *columnLength = rangeEnd - rangeStart;
    }
    return true;
}

bool setRangeFromTokenIndex(const TherionParsedLine &parsedLine,
                            int tokenIndex,
                            int *columnNumber,
                            int *columnLength)
{
    if (columnNumber != nullptr) {
        *columnNumber = 1;
    }
    if (columnLength != nullptr) {
        *columnLength = 0;
    }

    if (tokenIndex < 0) {
        return false;
    }

    const std::optional<TherionParsedToken> token = tokenSpanForCommandTokenIndex(parsedLine, tokenIndex);
    if (!token.has_value()) {
        return false;
    }

    if (token->start < 0 || token->length <= 0) {
        return false;
    }

    if (columnNumber != nullptr) {
        *columnNumber = token->start + 1;
    }
    if (columnLength != nullptr) {
        *columnLength = token->length;
    }
    return true;
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

QString cleanupInvalidSubtypeValues(const QString &lineText,
                                    bool *changed,
                                    int *firstChangedColumnNumber,
                                    int *firstChangedColumnLength)
{
    if (changed != nullptr) {
        *changed = false;
    }

    QString updated = lineText;
    for (;;) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(updated);
        const ParsedCommandOptions commandOptions = parseCommandOptions(parsedLine.directive,
                                                                        parsedLine.tokens,
                                                                        {},
                                                                        false,
                                                                        false);
        int removeStartTokenIndex = -1;
        int removeEndTokenIndex = -1;
        for (int entryIndex = 0; entryIndex < commandOptions.optionEntries.size(); ++entryIndex) {
            const CommandOptionEntry &entry = commandOptions.optionEntries.at(entryIndex);
            const QString optionToken = entry.key.trimmed().toLower();
            if (optionToken != QStringLiteral("-subtype")) {
                continue;
            }

            if (entry.firstValueTokenIndex >= 0 && entry.lastValueTokenIndex >= entry.firstValueTokenIndex) {
                for (int valueIndex = entry.firstValueTokenIndex; valueIndex <= entry.lastValueTokenIndex; ++valueIndex) {
                    if (valueIndex >= parsedLine.tokens.size()) {
                        break;
                    }
                    if (commandTokenEmbedsOptionValue(parsedLine.tokens.at(valueIndex))) {
                        removeStartTokenIndex = entry.optionTokenIndex;
                        removeEndTokenIndex = entry.lastValueTokenIndex;
                        break;
                    }
                }
            }

            if (removeStartTokenIndex < 0
                && entry.rawValueTokens.isEmpty()
                && entryIndex + 1 < commandOptions.optionEntries.size()) {
                const CommandOptionEntry &nextEntry = commandOptions.optionEntries.at(entryIndex + 1);
                if (nextEntry.embeddedValue) {
                    removeStartTokenIndex = entry.optionTokenIndex;
                    removeEndTokenIndex = nextEntry.optionTokenIndex;
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
        if (firstChangedColumnLength != nullptr
            && *firstChangedColumnLength <= 0) {
            setRangeFromTokenRange(updated,
                                   parsedLine,
                                   removeStartTokenIndex,
                                   removeEndTokenIndex,
                                   firstChangedColumnNumber,
                                   firstChangedColumnLength);
        }
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

QString cleanupDuplicateOptions(const QString &lineText,
                                bool *changed,
                                int *firstChangedColumnNumber,
                                int *firstChangedColumnLength)
{
    if (changed != nullptr) {
        *changed = false;
    }

    QString updated = lineText;
    for (;;) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(updated);
        const ParsedCommandOptions commandOptions = parseCommandOptions(parsedLine.directive,
                                                                        parsedLine.tokens,
                                                                        {},
                                                                        false,
                                                                        false);
        QStringList seenOptions;
        int removeStartTokenIndex = -1;
        int removeEndTokenIndex = -1;

        for (const CommandOptionEntry &entry : commandOptions.optionEntries) {
            const QString optionToken = entry.key;
            const QStringList values = entry.rawValueTokens;
            const QString deduplicationKey = optionDeduplicationKey(optionToken, values);
            if (seenOptions.contains(deduplicationKey)) {
                removeStartTokenIndex = entry.optionTokenIndex;
                removeEndTokenIndex = entry.embeddedValue ? entry.optionTokenIndex : entry.lastValueTokenIndex;
                break;
            }

            seenOptions.append(deduplicationKey);
        }

        if (removeStartTokenIndex < 0) {
            return updated;
        }

        bool removed = false;
        if (firstChangedColumnLength != nullptr
            && *firstChangedColumnLength <= 0) {
            setRangeFromTokenRange(updated,
                                   parsedLine,
                                   removeStartTokenIndex,
                                   removeEndTokenIndex,
                                   firstChangedColumnNumber,
                                   firstChangedColumnLength);
        }
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

LineCleanupResult cleanupLine(const QString &lineText)
{
    LineCleanupResult result;
    result.text = lineText;

    bool subtypeChanged = false;
    result.text = cleanupInvalidSubtypeValues(result.text,
                                              &subtypeChanged,
                                              &result.columnNumber,
                                              &result.columnLength);

    bool duplicateChanged = false;
    result.text = cleanupDuplicateOptions(result.text,
                                          &duplicateChanged,
                                          &result.columnNumber,
                                          &result.columnLength);

    result.changed = subtypeChanged || duplicateChanged;
    return result;
}

TherionSourceDiagnostic diagnosticForLineCleanup(const TherionSourceLogicalCommand &command,
                                                 const LineCleanupResult &cleanup)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = QStringLiteral("malformed-option-token");
    diagnostic.severity = TherionSourceDiagnosticSeverity::Error;
    diagnostic.lineNumber = command.startLineNumber;
    diagnostic.columnNumber = cleanup.columnNumber;
    diagnostic.columnLength = cleanup.columnLength;
    diagnostic.title = QStringLiteral("Malformed or duplicate option token");
    diagnostic.message = QStringLiteral("This command contains an option-like token or duplicate option/value pair that Therion may reject.");
    diagnostic.currentText = command.text;
    diagnostic.suggestedText = cleanup.text;
    diagnostic.hasFix = true;
    diagnostic.fix.startOffset = command.startOffset;
    diagnostic.fix.length = command.text.size();
    diagnostic.fix.replacementText = cleanup.text;
    diagnostic.fix.description = QStringLiteral("Rewrite line %1").arg(command.startLineNumber);
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

TherionSourceDiagnostic diagnosticForLine(const TherionSourceLogicalCommand &command,
                                          const QString &code,
                                          const QString &title,
                                          const QString &message,
                                          TherionSourceDiagnosticSeverity severity = TherionSourceDiagnosticSeverity::Warning)
{
    TherionSourceDiagnostic diagnostic;
    diagnostic.code = code;
    diagnostic.severity = severity;
    diagnostic.lineNumber = command.startLineNumber;
    diagnostic.columnNumber = 1;
    diagnostic.title = title;
    diagnostic.message = message;
    diagnostic.currentText = command.text;
    diagnostic.suggestedText = QString();
    diagnostic.hasFix = false;
    return diagnostic;
}

TherionSourceDiagnostic diagnosticForToken(const TherionSourceLogicalCommand &command,
                                           int tokenIndex,
                                           const QString &code,
                                           const QString &title,
                                           const QString &message,
                                           TherionSourceDiagnosticSeverity severity = TherionSourceDiagnosticSeverity::Warning)
{
    TherionSourceDiagnostic diagnostic = diagnosticForLine(command,
                                                          code,
                                                          title,
                                                          message,
                                                          severity);
    TherionSourcePhysicalRange physicalRange;
    if (command.physicalRangeForTokenIndex(tokenIndex, &physicalRange)) {
        diagnostic.lineNumber = physicalRange.lineNumber;
        diagnostic.columnNumber = physicalRange.columnNumber;
        diagnostic.columnLength = physicalRange.columnLength;
        diagnostic.currentText = physicalRange.lineText;
        return diagnostic;
    }

    setRangeFromTokenIndex(command.parsed,
                           tokenIndex,
                           &diagnostic.columnNumber,
                           &diagnostic.columnLength);
    return diagnostic;
}

void appendCommandCatalogDiagnostics(TherionSourceValidationResult *result,
                                     const TherionSourceLogicalCommand &command)
{
    if (result == nullptr || command.parsed.tokens.isEmpty()) {
        return;
    }

    const QString commandName = command.metadata.commandName;
    const bool commandKnown = command.metadata.catalogCommandKnown;
    if (!commandKnown) {
        if (!therionDirectiveIsKnownBlockDirective(commandName)
            && looksLikeCommandDirective(command.parsed.directive)) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          0,
                                                          QStringLiteral("unknown-command"),
                                                          QStringLiteral("Unknown command"),
                                                          QStringLiteral("Command `%1` is not present in the Therion command catalog.").arg(command.parsed.directive)));
        }
        return;
    }

    const int requiredPositionalCount = command.metadata.catalogRequiredPositionalCount;
    const int providedPositionalCount = command.metadata.positionalArgumentCount;
    if (requiredPositionalCount > 0 && providedPositionalCount < requiredPositionalCount) {
        result->diagnostics.append(diagnosticForLine(command,
                                                     QStringLiteral("missing-argument"),
                                                     QStringLiteral("Missing argument"),
                                                     QStringLiteral("Command `%1` expects at least %2 positional argument(s), but %3 provided.")
                                                         .arg(commandName)
                                                         .arg(requiredPositionalCount)
                                                         .arg(providedPositionalCount),
                                                     TherionSourceDiagnosticSeverity::Error));
    }

    const QStringList allowedFirstValues =
        command.metadata.catalogArgumentAllowedValuesByIndex.value(0);
    if (!allowedFirstValues.isEmpty() && providedPositionalCount > 0 && command.parsed.tokens.size() > 1) {
        const QString value = command.parsed.tokens.at(1).trimmed();
        if (!value.isEmpty() && !allowedFirstValues.contains(value, Qt::CaseInsensitive)) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          1,
                                                          QStringLiteral("unknown-argument-value"),
                                                          QStringLiteral("Unknown argument value"),
                                                          QStringLiteral("Command `%1` does not list `%2` as a known value. Known values: %3.")
                                                              .arg(commandName, value, allowedFirstValues.join(QStringLiteral(", ")))));
        }
    }

    const QSet<QString> knownOptions = command.metadata.catalogOptionNames;
    for (const TherionSourceLogicalOptionEntryRange &optionEntry : command.optionEntryRanges) {
        const QString optionToken = optionEntry.key;
        const QString normalizedOption = QStringLiteral("-") + normalizedCommandOptionName(optionToken);
        if (!knownOptions.isEmpty() && !knownOptions.contains(normalizedOption)) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          optionEntry.optionTokenIndex,
                                                          QStringLiteral("unknown-option"),
                                                          QStringLiteral("Unknown option"),
                                                          QStringLiteral("Command `%1` does not list option `%2` in the Therion command catalog.")
                                                              .arg(commandName, optionToken)));
        }

        const QString arity = command.metadata.catalogOptionValueArityTokens.value(normalizedOption);
        const int fixedArity = command.metadata.catalogOptionFixedArityByName.value(normalizedOption, -1);
        const int providedValueCount = optionEntry.logicalValueCount;
        if ((optionArityRequiresValue(arity) || fixedArity > 0) && providedValueCount == 0) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          optionEntry.optionTokenIndex,
                                                          QStringLiteral("missing-option-value"),
                                                          QStringLiteral("Missing option value"),
                                                          QStringLiteral("Option `%1` on command `%2` expects a value.")
                                                              .arg(optionToken, commandName),
                                                          TherionSourceDiagnosticSeverity::Error));
        } else if (fixedArity > 0 && providedValueCount > 0 && providedValueCount != fixedArity) {
            result->diagnostics.append(diagnosticForToken(command,
                                                          optionEntry.optionTokenIndex,
                                                          QStringLiteral("wrong-option-value-count"),
                                                          QStringLiteral("Unexpected option value count"),
                                                          QStringLiteral("Option `%1` on command `%2` expects exactly %3 value(s), but %4 provided.")
                                                              .arg(optionToken)
                                                              .arg(commandName)
                                                              .arg(fixedArity)
                                                              .arg(providedValueCount),
                                                          TherionSourceDiagnosticSeverity::Error));
        }

        const QStringList allowedOptionValues =
            command.metadata.catalogOptionAllowedValuesByName.value(normalizedOption);
        if (!allowedOptionValues.isEmpty()
            && optionEntry.logicalValueCount == 1
            && optionEntry.rawValueTokens.size() == 1) {
            const QString valueToken = optionEntry.rawValueTokens.constFirst().trimmed();
            if (!valueToken.isEmpty()
                && !allowedOptionValues.contains(valueToken, Qt::CaseInsensitive)) {
                const int diagnosticTokenIndex = optionEntry.embeddedValue
                    ? optionEntry.optionTokenIndex
                    : optionEntry.firstValueTokenIndex;
                result->diagnostics.append(diagnosticForToken(command,
                                                              diagnosticTokenIndex,
                                                              QStringLiteral("unknown-option-value"),
                                                              QStringLiteral("Unknown option value"),
                                                              QStringLiteral("Option `%1` on command `%2` does not list `%3` as a known value. Known values: %4.")
                                                                  .arg(normalizedOption, commandName, valueToken, allowedOptionValues.join(QStringLiteral(", ")))));
            }
        }
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
    const TherionSourceLogicalDocument logicalDocument =
        catalog.commandNames.isEmpty()
            ? TherionSourceLogicalDocument::fromSourceDocument(sourceDocument)
            : TherionSourceLogicalDocument::fromSourceDocument(sourceDocument, catalog);
    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        if (command.startLineNumber == command.endLineNumber) {
            const LineCleanupResult cleanup = cleanupLine(command.text);
            if (cleanup.changed && cleanup.text != command.text) {
                result.diagnostics.append(diagnosticForLineCleanup(command, cleanup));
            }
        }

        if (!catalog.commandNames.isEmpty() && command.shouldValidateCommandCatalog()) {
            appendCommandCatalogDiagnostics(&result, command);
        }
    }

    if (!catalog.commandNames.isEmpty()) {
        appendBlockDiagnostics(&result, sourceDocument);
    }
    return result;
}

QVector<TherionSourceTextEdit> TherionSourceValidator::validationFixEdits(
    const QString &contents,
    const QVector<TherionSourceDiagnosticFix> &fixes)
{
    QVector<TherionSourceDiagnosticFix> sortedFixes = fixes;
    std::sort(sortedFixes.begin(),
              sortedFixes.end(),
              [](const TherionSourceDiagnosticFix &left, const TherionSourceDiagnosticFix &right) {
                  return left.startOffset > right.startOffset;
              });

    QVector<TherionSourceTextEdit> edits;
    edits.reserve(sortedFixes.size());
    for (const TherionSourceDiagnosticFix &fix : std::as_const(sortedFixes)) {
        if (fix.startOffset < 0
            || fix.length < 0
            || fix.startOffset + fix.length > contents.size()) {
            continue;
        }
        edits.append(TherionSourceTextEdit{fix.startOffset, fix.length, fix.replacementText});
    }
    return edits;
}

QString TherionSourceValidator::applyFixes(const QString &contents,
                                           const QVector<TherionSourceDiagnosticFix> &fixes)
{
    const QVector<TherionSourceTextEdit> edits = validationFixEdits(contents, fixes);

    QString updated = contents;
    for (const TherionSourceTextEdit &edit : edits) {
        updated.replace(edit.startOffset, edit.length, edit.replacementText);
    }
    return updated;
}
}
