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

int runLosslessSourceProjectionLineNumberTest()
{
    const QString text = QStringLiteral(
        "# header\r\n"
        "\r\n"
        "scrap s1 -projection plan\r\n"
        "line border -id line-1 -close on\r\n"
        "  0 0\r\n"
        "endline\r\n"
        "  # area comment\r\n"
        "area water -id a1\r\n"
        "  line-1\r\n"
        "endarea\r\n"
        "endscrap\r\n");

    const QSet<int> borderLines = mapEditorBorderLineNumbersForArea(text, 8);
    if (!expect(borderLines.size() == 1 && borderLines.contains(4),
                "Area reference lookup should preserve physical line numbers through the lossless projection.")) {
        return 1;
    }

    const QVector<MapEditorAreaReference> references = mapEditorAreaReferencesForBorderLine(text, 4);
    if (!expect(references.size() == 1,
                "Border line lookup should resolve through the lossless projection.")) {
        return 1;
    }
    return expect(references.first().areaLineNumber == 8,
                  "Border line lookup should report physical area line numbers after blank/comment lines.")
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
    if (const int rc = runLosslessSourceProjectionLineNumberTest(); rc != 0) {
        return rc;
    }
    return 0;
}
