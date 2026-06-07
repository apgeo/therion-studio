#include "../src/app/text_editor/block_editor/BlockEditorDirectiveRules.h"
#include "../src/app/text_editor/block_editor/BlockEditorDocumentOutlineBuilder.h"

#include <QString>

#include <iostream>

using namespace TherionStudio;
using namespace TherionStudio::BlockEditorDirectiveRules;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

BlockEditorDocumentOutlineContext outlineContext()
{
    BlockEditorDocumentOutlineContext context;
    context.resolveScopeForCommandAtLine = [](const QString &command, const QStringList &, int lineNumber) {
        return normalizeDirective(command) == QStringLiteral("data") && lineNumber == 5
            ? QStringLiteral("centerline")
            : QStringLiteral("none");
    };
    context.isContainerDirectiveInstanceForParsedLine = [](const QString &directive, const TherionParsedLine &parsedLine) {
        return isContainerDirectiveInstance(directive, parsedLine);
    };
    context.isCommandDirectiveInScope = [](const QString &directive, const QString &scope) {
        const QString normalizedDirective = normalizeDirective(directive);
        const QString normalizedScope = normalizeDirective(scope);
        if (normalizedScope == QStringLiteral("none")) {
            return normalizedDirective == QStringLiteral("encoding");
        }
        if (normalizedScope == QStringLiteral("centerline")) {
            return normalizedDirective == QStringLiteral("date")
                || normalizedDirective == QStringLiteral("data")
                || normalizedDirective == QStringLiteral("extend")
                || normalizedDirective == QStringLiteral("team");
        }
        return false;
    };
    return context;
}

int runDataEntryConsumesExtendRowsTest()
{
    const QString contents = QStringLiteral(
        "encoding utf-8\n"
        "survey test\n"
        "centerline\n"
        "  date 2006.08.12\n"
        "  data normal from to compass clino tape\n"
        "  extend right\n"
        "  2.26 2.33 48.11 48.42 3.040\n"
        "  extend left\n"
        "  2.36 2.43 154.75 12.26 1.516\n"
        "  team surveyor\n"
        "endcenterline\n"
        "endsurvey\n");

    const BlockEditorDocumentOutline outline = BlockEditorDocumentOutlineBuilder(outlineContext()).buildFromContents(contents);
    const auto dataEntryIndex = outline.entryIndexByStartLine.constFind(5);
    if (!expect(dataEntryIndex != outline.entryIndexByStartLine.constEnd(),
                "Outline should contain the centerline data block entry.")) {
        return 1;
    }

    const BlockEditorDocumentEntry dataEntry = outline.entries.at(*dataEntryIndex);
    if (!expect(dataEntry.kind == QStringLiteral("data"),
                "Data outline entry should keep kind data.")) {
        return 1;
    }
    if (!expect(dataEntry.startLine == 5 && dataEntry.endLine == 9,
                "Data outline entry should consume extend markers and measurement rows until the next centerline command.")) {
        return 1;
    }

    for (const BlockEditorDocumentEntry &entry : outline.entries) {
        if (!expect(entry.kind != QStringLiteral("extend"),
                    "Extend rows inside a data block should not become standalone block entries.")) {
            return 1;
        }
    }

    const auto teamEntryIndex = outline.entryIndexByStartLine.constFind(10);
    if (!expect(teamEntryIndex != outline.entryIndexByStartLine.constEnd(),
                "The first command after the data body should still become its own entry.")) {
        return 1;
    }
    return expect(outline.entries.at(*teamEntryIndex).kind == QStringLiteral("team"),
                  "The command after the data body should be parsed as a team entry.")
        ? 0
        : 1;
}
}

int main()
{
    if (runDataEntryConsumesExtendRowsTest() != 0) {
        return 1;
    }

    return 0;
}
