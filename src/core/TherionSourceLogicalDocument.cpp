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

QString stripTrailingContinuationMarker(const QString &line)
{
    QString trimmed = rightTrimmed(line);
    if (!trimmed.isEmpty() && trimmed.back() == QLatin1Char('\\')) {
        trimmed.chop(1);
    }
    return trimmed;
}

QString appendLogicalCommandPart(QString logicalText, const QString &sourceLineText, bool previousLineContinued)
{
    const bool lineContinues = endsWithContinuationMarker(sourceLineText);
    const QString currentPart = lineContinues
        ? stripTrailingContinuationMarker(sourceLineText)
        : sourceLineText;
    const QString normalizedPart = previousLineContinued
        ? currentPart.trimmed()
        : currentPart;

    if (!normalizedPart.isEmpty()) {
        if (!logicalText.isEmpty()) {
            logicalText.append(QLatin1Char(' '));
        }
        logicalText.append(normalizedPart);
    }
    return logicalText;
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
            command.text = appendLogicalCommandPart(command.text,
                                                    currentLine.sourceLine.text,
                                                    previousLineContinued);
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
