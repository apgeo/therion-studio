#include "BlockEditorSourceText.h"

#include "../../../core/TherionSourceText.h"

#include <QtGlobal>

namespace TherionStudio
{
namespace
{
QString rightTrimmed(const QString &line)
{
    QString trimmed = line;
    while (!trimmed.isEmpty() && trimmed.back().isSpace()) {
        trimmed.chop(1);
    }
    return trimmed;
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
}

QString blockEditorSourceLineEnding(const QString &contents)
{
    return TherionSourceText::detectedLineEnding(contents);
}

QStringList blockEditorNormalizedSourceLines(const QString &contents)
{
    return TherionSourceText::splitTextLines(contents);
}

QString blockEditorJoinSourceLines(const QString &originalContents, const QStringList &lines)
{
    return TherionSourceText::joinTextLines(originalContents, lines);
}

QVector<BlockEditorLogicalLine> blockEditorBuildLogicalLines(const QStringList &lines)
{
    QVector<BlockEditorLogicalLine> logicalLines;
    logicalLines.reserve(lines.size());

    int lineIndex = 0;
    while (lineIndex < lines.size()) {
        const int startLine = lineIndex + 1;
        int endLine = startLine;
        QString logicalText;
        bool hasContinuation = false;

        int currentIndex = lineIndex;
        while (currentIndex < lines.size()) {
            const QString currentLine = lines.at(currentIndex);
            const bool lineContinues = endsWithContinuationMarker(currentLine);
            const QString currentPart = lineContinues
                ? stripTrailingContinuationMarker(currentLine)
                : currentLine;
            const QString normalizedPart = hasContinuation
                ? currentPart.trimmed()
                : currentPart;
            if (!normalizedPart.isEmpty()) {
                if (!logicalText.isEmpty()) {
                    logicalText.append(QLatin1Char(' '));
                }
                logicalText.append(normalizedPart);
            }

            hasContinuation = lineContinues;
            endLine = currentIndex + 1;
            ++currentIndex;
            if (!lineContinues) {
                break;
            }
        }

        logicalLines.append(BlockEditorLogicalLine{
            startLine,
            endLine,
            logicalText
        });
        lineIndex = currentIndex;
    }

    return logicalLines;
}

bool blockEditorResolveLogicalLineAtLine(const QStringList &lines,
                                         int lineNumber,
                                         BlockEditorLogicalLine *logicalLine)
{
    if (lineNumber <= 0 || lineNumber > lines.size() || logicalLine == nullptr) {
        return false;
    }

    const QVector<BlockEditorLogicalLine> logicalLines = blockEditorBuildLogicalLines(lines);
    for (const BlockEditorLogicalLine &candidate : logicalLines) {
        if (lineNumber < candidate.startLine || lineNumber > candidate.endLine) {
            continue;
        }
        *logicalLine = candidate;
        return true;
    }

    return false;
}

bool blockEditorReplaceSourceLineRange(QStringList *lines,
                                       int startLine,
                                       int endLine,
                                       const QStringList &replacementLines)
{
    if (lines == nullptr || startLine <= 0 || endLine < startLine - 1) {
        return false;
    }

    const int removeStartIndex = startLine - 1;
    const int removeEndIndex = endLine - 1;
    if (removeStartIndex < 0 || removeStartIndex > lines->size()) {
        return false;
    }
    if (removeEndIndex >= lines->size()) {
        return false;
    }

    if (endLine >= startLine) {
        for (int index = removeEndIndex; index >= removeStartIndex; --index) {
            lines->removeAt(index);
        }
    }
    for (int offset = 0; offset < replacementLines.size(); ++offset) {
        lines->insert(removeStartIndex + offset, replacementLines.at(offset));
    }
    return true;
}
}
