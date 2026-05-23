#include "../src/app/text_editor/map_editor/MapEditorAreaReferenceResolver.h"

#include <QString>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runAreaToBorderLineResolutionTest()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "endline\n"
        "area water -id a1\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");

    const QSet<int> borderLines = mapEditorBorderLineNumbersForArea(text, 5);
    if (!expect(borderLines.size() == 1 && borderLines.contains(2),
                "Area reference lookup should resolve body line IDs to border source lines.")) {
        return 1;
    }

    const QVector<MapEditorAreaReference> references = mapEditorAreaReferencesForBorderLine(text, 2);
    if (!expect(references.size() == 1,
                "Border line reference lookup should find the owning area.")) {
        return 1;
    }
    if (!expect(references.first().areaLineNumber == 5,
                "Border line reference lookup should report the owning area source line.")) {
        return 1;
    }
    return expect(references.first().areaLabel == QStringLiteral("water (a1)"),
                  "Border line reference lookup should expose a user-facing area label.")
        ? 0
        : 1;
}

int runMultipleAreaReferenceTest()
{
    const QString text = QStringLiteral(
        "scrap s1 -projection plan\n"
        "line border -id line-1 -close on\n"
        "  0 0\n"
        "endline\n"
        "area water\n"
        "  line-1\n"
        "endarea\n"
        "area sand\n"
        "  line-1\n"
        "endarea\n"
        "endscrap\n");

    const QVector<MapEditorAreaReference> references = mapEditorAreaReferencesForBorderLine(text, 2);
    if (!expect(references.size() == 2,
                "Shared border line lookup should report every referencing area.")) {
        return 1;
    }
    return expect(references.at(0).areaLineNumber == 5 && references.at(1).areaLineNumber == 8,
                  "Shared border line lookup should preserve source order for area links.")
        ? 0
        : 1;
}
}

int main()
{
    if (const int rc = runAreaToBorderLineResolutionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runMultipleAreaReferenceTest(); rc != 0) {
        return rc;
    }
    return 0;
}
