#include "BlockEditorDocumentOutlineBuilder.h"

#include "BlockEditorDirectiveRules.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionDocumentParser.h"

namespace
{
struct OpenContainer
{
    QString directive;
    int lineNumber = 0;
};

int nearestOpenContainerLine(const QVector<OpenContainer> &openStack, const QString &directive)
{
    for (int index = openStack.size() - 1; index >= 0; --index) {
        if (openStack.at(index).directive == directive) {
            return openStack.at(index).lineNumber;
        }
    }
    return 0;
}
}

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

BlockEditorDocumentOutlineBuilder::BlockEditorDocumentOutlineBuilder(const TextEditorTab *owner)
    : owner_(owner)
{
}

BlockEditorDocumentOutline BlockEditorDocumentOutlineBuilder::buildFromContents(const QString &contents) const
{
    BlockEditorDocumentOutline outline;
    outline.lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : outline.lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    if (owner_ == nullptr) {
        return outline;
    }

    QVector<TherionParsedLine> parsedLines;
    parsedLines.reserve(outline.lines.size());
    for (int lineIndex = 0; lineIndex < outline.lines.size(); ++lineIndex) {
        parsedLines.append(TherionDocumentParser::parseLine(outline.lines.at(lineIndex), lineIndex + 1));
    }

    outline.entries.reserve(parsedLines.size());
    QVector<OpenContainer> openStack;
    const QString dataScope = owner_->resolveScopeForCommandAtLine(QStringLiteral("data"),
                                                                    outline.lines,
                                                                    outline.lines.size() + 1);
    const QString dataScopeClosing = completionClosingDirectiveForOpening(dataScope);

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const int currentLineNumber = parsedLine.lineNumber;
        if (isFullLineComment(parsedLine)) {
            BlockEditorDocumentEntry entry;
            entry.kind = QStringLiteral("comment");
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            outline.entryIndexByStartLine.insert(entry.startLine, outline.entries.size());
            outline.entries.append(entry);
            continue;
        }

        const QString directive = normalizeDirective(parsedLine.directive);
        if (directive.isEmpty()) {
            continue;
        }

        if (isBlockClosingDirective(directive)) {
            const QString openingDirective = completionOpeningDirectiveForClosing(directive);
            if (openingDirective.isEmpty()) {
                continue;
            }
            for (int index = openStack.size() - 1; index >= 0; --index) {
                if (openStack.at(index).directive == openingDirective) {
                    openStack.remove(index, openStack.size() - index);
                    break;
                }
            }
            continue;
        }

        if (isBlockOpeningDirective(directive)) {
            int parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            if (directive == QStringLiteral("data")) {
                const int dataScopeParentLine = nearestOpenContainerLine(openStack, dataScope);
                if (dataScopeParentLine > 0) {
                    parentLine = dataScopeParentLine;
                }
            }

            BlockEditorDocumentEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = parentLine;

            if (owner_->isContainerDirectiveInstanceForParsedLine(directive, parsedLine)) {
                const QString closingDirective = closingDirectiveFor(directive);
                const int endLine = findMatchingBlockEndLine(outline.lines,
                                                             currentLineNumber,
                                                             directive,
                                                             closingDirective);
                if (endLine > currentLineNumber) {
                    entry.endLine = endLine;
                }
                openStack.append(OpenContainer{directive, currentLineNumber});
            } else if (directive == QStringLiteral("data")) {
                const int dataScopeParentLine = nearestOpenContainerLine(openStack, dataScope);
                int dataScopeEndLine = outline.lines.size() + 1;
                if (dataScopeParentLine > 0) {
                    const int resolvedEndLine = findMatchingBlockEndLine(outline.lines,
                                                                         dataScopeParentLine,
                                                                         dataScope,
                                                                         dataScopeClosing);
                    if (resolvedEndLine > dataScopeParentLine) {
                        dataScopeEndLine = resolvedEndLine;
                    }
                }

                int dataBodyLastLine = dataScopeEndLine - 1;
                for (int scanLine = currentLineNumber + 1; scanLine <= dataScopeEndLine - 1; ++scanLine) {
                    const TherionParsedLine scanParsedLine = TherionDocumentParser::parseLine(outline.lines.at(scanLine - 1),
                                                                                               scanLine);
                    const QString scanDirective = normalizeDirective(scanParsedLine.directive);
                    if (scanDirective.isEmpty() || scanDirective == QStringLiteral("extend")) {
                        continue;
                    }
                    if (scanDirective == QStringLiteral("data")
                        || (!dataScopeClosing.isEmpty() && scanDirective == dataScopeClosing)
                        || (!dataScope.isEmpty() && owner_->isCommandDirectiveInScope(scanDirective, dataScope))) {
                        dataBodyLastLine = scanLine - 1;
                        break;
                    }
                }
                if (dataBodyLastLine < currentLineNumber) {
                    dataBodyLastLine = currentLineNumber;
                }
                entry.endLine = dataBodyLastLine;
            }

            outline.entryIndexByStartLine.insert(entry.startLine, outline.entries.size());
            outline.entries.append(entry);
            continue;
        }

        QString activeScope = QStringLiteral("none");
        if (!openStack.isEmpty()) {
            activeScope = normalizeDirective(openStack.last().directive);
        }

        const bool commandDirective = !isBlockOpeningDirective(directive)
            && owner_->isCommandDirectiveInScope(directive, activeScope);
        if (commandDirective) {
            BlockEditorDocumentEntry entry;
            entry.kind = directive;
            entry.startLine = currentLineNumber;
            entry.endLine = currentLineNumber;
            entry.parentLine = openStack.isEmpty() ? 0 : openStack.last().lineNumber;
            outline.entryIndexByStartLine.insert(entry.startLine, outline.entries.size());
            outline.entries.append(entry);
        }
    }

    return outline;
}
}
