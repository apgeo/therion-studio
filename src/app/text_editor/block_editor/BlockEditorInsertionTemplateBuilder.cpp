#include "BlockEditorInsertionTemplateBuilder.h"

#include "BlockEditorDirectiveRules.h"

#include <QRegularExpression>

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

QStringList BlockEditorInsertionTemplateBuilder::buildLines(const QString &normalizedKind,
                                                            const QStringList &existingLines,
                                                            const BlockEditorInsertionPlan &insertionPlan) const
{
    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto lineIndent = [&existingLines, &indentPattern](int lineNumber) {
        if (lineNumber <= 0 || lineNumber > existingLines.size()) {
            return QString();
        }
        const auto match = indentPattern.match(existingLines.at(lineNumber - 1));
        return match.hasMatch() ? match.captured(0) : QString();
    };

    const QString indent = insertionPlan.parentLine > 0
        ? lineIndent(insertionPlan.parentLine)
            + (isContainerBlockDirective(insertionPlan.parentKind) ? QStringLiteral("  ") : QString())
        : QString();

    QStringList linesToInsert;
    if (normalizedKind == QStringLiteral("comment")) {
        linesToInsert << QStringLiteral("%1#").arg(indent);
    } else if (isMapObjectReferenceKind(normalizedKind)) {
        linesToInsert << QStringLiteral("%1new-object").arg(indent);
    } else if (normalizedKind == QStringLiteral("data")) {
        linesToInsert << QStringLiteral("%1data").arg(indent);
    } else if (isContainerBlockDirective(normalizedKind)) {
        linesToInsert << QStringLiteral("%1%2").arg(indent, normalizedKind);
        const QString closingDirective = completionClosingDirectiveForOpening(normalizedKind);
        if (!closingDirective.isEmpty()) {
            linesToInsert << QStringLiteral("%1%2").arg(indent, closingDirective);
        }
    } else {
        linesToInsert << QStringLiteral("%1%2").arg(indent, normalizedKind);
    }

    if (linesToInsert.isEmpty()) {
        linesToInsert << QStringLiteral("%1%2").arg(indent, normalizedKind);
    }

    return linesToInsert;
}
}
