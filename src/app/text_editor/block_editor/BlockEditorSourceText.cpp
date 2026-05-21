#include "BlockEditorSourceText.h"

#include <QtGlobal>

namespace TherionStudio
{
QString blockEditorSourceLineEnding(const QString &contents)
{
    return contents.contains(QStringLiteral("\r\n"))
        ? QStringLiteral("\r\n")
        : QStringLiteral("\n");
}

QStringList blockEditorNormalizedSourceLines(const QString &contents)
{
    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    return lines;
}

QString blockEditorJoinSourceLines(const QString &originalContents, const QStringList &lines)
{
    return lines.join(blockEditorSourceLineEnding(originalContents));
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
