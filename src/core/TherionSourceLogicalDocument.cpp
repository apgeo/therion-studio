#include "TherionSourceLogicalDocument.h"

namespace TherionStudio
{
namespace
{
QString rightTrimmed(QString line)
{
    while (!line.isEmpty() && line.back().isSpace()) {
        line.chop(1);
    }
    return line;
}

bool endsWithContinuationMarker(const QString &line)
{
    const QString trimmed = rightTrimmed(line);
    if (trimmed.isEmpty()) {
        return false;
    }

    int trailingBackslashCount = 0;
    int index = trimmed.size() - 1;
    while (index >= 0 && trimmed.at(index) == QLatin1Char('\\')) {
        ++trailingBackslashCount;
        --index;
    }

    return trailingBackslashCount % 2 == 1;
}

int firstNonSpaceIndex(const QString &line)
{
    for (int index = 0; index < line.size(); ++index) {
        if (!line.at(index).isSpace()) {
            return index;
        }
    }
    return line.size();
}

QString stripTrailingContinuationMarker(const QString &line)
{
    QString trimmed = rightTrimmed(line);
    if (!trimmed.isEmpty() && trimmed.back() == QLatin1Char('\\')) {
        trimmed.chop(1);
    }
    return trimmed;
}

struct LogicalCommandPart
{
    QString text;
    int sourceColumnNumber = 1;
    int sourceStartOffsetDelta = 0;
};

LogicalCommandPart logicalCommandPartForLine(const QString &sourceLineText, bool previousLineContinued)
{
    const bool lineContinues = endsWithContinuationMarker(sourceLineText);
    const QString currentPart = lineContinues
        ? stripTrailingContinuationMarker(sourceLineText)
        : sourceLineText;
    if (!previousLineContinued) {
        return {currentPart, 1, 0};
    }

    const int firstContentIndex = firstNonSpaceIndex(currentPart);
    const QString normalizedPart = currentPart.mid(firstContentIndex).trimmed();
    if (normalizedPart.isEmpty()) {
        return {};
    }
    return {normalizedPart, firstContentIndex + 1, firstContentIndex};
}
}

bool TherionSourceLogicalCommand::shouldValidateCommandCatalog() const
{
    return role == TherionSourceLineRole::Command;
}

bool TherionSourceLogicalCommand::hasUnmatchedClose() const
{
    if (!closesBlock) {
        return false;
    }
    return blockStackBefore.isEmpty()
        || blockStackBefore.constLast().directive != closeMatchesOpenDirective;
}

bool TherionSourceLogicalCommand::physicalRangeForLogicalRange(int logicalStart,
                                                               int logicalLength,
                                                               TherionSourcePhysicalRange *range) const
{
    if (range != nullptr) {
        *range = {};
    }
    if (logicalStart < 0 || logicalLength <= 0) {
        return false;
    }

    for (const TherionSourceLogicalTextPart &part : textParts) {
        const int partStart = part.logicalStart;
        const int partEnd = part.logicalStart + part.logicalLength;
        if (logicalStart < partStart || logicalStart >= partEnd) {
            continue;
        }

        const int delta = logicalStart - partStart;
        const int mappedLength = qMin(logicalLength, partEnd - logicalStart);
        if (range != nullptr) {
            range->lineNumber = part.lineNumber;
            range->columnNumber = part.columnNumber + delta;
            range->columnLength = mappedLength;
            range->startOffset = part.startOffset + delta;
            range->length = mappedLength;
            range->lineText = part.lineText;
        }
        return true;
    }

    return false;
}

TherionSourceLogicalDocument TherionSourceLogicalDocument::fromText(const QString &contents)
{
    return fromSourceDocument(TherionSourceDocument::fromText(contents));
}

TherionSourceLogicalDocument TherionSourceLogicalDocument::fromSourceDocument(const TherionSourceDocument &sourceDocument)
{
    TherionSourceLogicalDocument logicalDocument;
    const QVector<TherionSourceDocumentLine> &lines = sourceDocument.lines();
    logicalDocument.commands_.reserve(lines.size());

    int lineIndex = 0;
    while (lineIndex < lines.size()) {
        const TherionSourceDocumentLine &firstLine = lines.at(lineIndex);
        TherionSourceLogicalCommand command;
        command.startLineNumber = firstLine.sourceLine.lineNumber;
        command.endLineNumber = firstLine.sourceLine.lineNumber;
        command.startOffset = firstLine.sourceLine.startOffset;
        command.endOffset = firstLine.sourceLine.endOffset;
        command.currentBlockDirective = firstLine.currentBlockDirective;
        command.role = firstLine.role;
        command.opensBlock = firstLine.opensBlock;
        command.closesBlock = firstLine.closesBlock;
        command.closeMatchesOpenDirective = firstLine.closeMatchesOpenDirective;
        command.blockStackBefore = firstLine.blockStackBefore;

        bool previousLineContinued = false;
        int currentIndex = lineIndex;
        while (currentIndex < lines.size()) {
            const TherionSourceDocumentLine &currentLine = lines.at(currentIndex);
            const LogicalCommandPart part = logicalCommandPartForLine(currentLine.sourceLine.text,
                                                                      previousLineContinued);
            if (!part.text.isEmpty()) {
                if (!command.text.isEmpty()) {
                    command.text.append(QLatin1Char(' '));
                }

                TherionSourceLogicalTextPart textPart;
                textPart.logicalStart = command.text.size();
                textPart.logicalLength = part.text.size();
                textPart.lineNumber = currentLine.sourceLine.lineNumber;
                textPart.columnNumber = part.sourceColumnNumber;
                textPart.startOffset = currentLine.sourceLine.startOffset + part.sourceStartOffsetDelta;
                textPart.lineText = currentLine.sourceLine.text;
                command.textParts.append(textPart);

                command.text.append(part.text);
            }
            command.endLineNumber = currentLine.sourceLine.lineNumber;
            command.endOffset = currentLine.sourceLine.endOffset;
            command.physicalLineNumbers.append(currentLine.sourceLine.lineNumber);

            const bool lineContinues = endsWithContinuationMarker(currentLine.sourceLine.text);
            previousLineContinued = lineContinues;
            ++currentIndex;
            if (!lineContinues) {
                break;
            }
        }

        command.parsed = TherionDocumentParser::parseLine(command.text, command.startLineNumber);
        command.normalizedDirective = normalizedTherionDirectiveToken(command.parsed.directive);
        if (command.parsed.tokens.isEmpty()) {
            command.normalizedDirective.clear();
        }
        logicalDocument.commands_.append(command);
        lineIndex = currentIndex;
    }

    return logicalDocument;
}

const QVector<TherionSourceLogicalCommand> &TherionSourceLogicalDocument::commands() const
{
    return commands_;
}
}
