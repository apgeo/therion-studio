#include "../src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.h"

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

int runLineFeatureLookupPreservesPhysicalLineNumbersTest()
{
    const QString text = QStringLiteral(
        "# header\r\n"
        "\r\n"
        "scrap s1 -projection plan\r\n"
        "  # line comment\r\n"
        "line wall -id wall-1\r\n"
        "  0 0\r\n"
        "  10 0\r\n"
        "endline\r\n"
        "endscrap\r\n");

    const std::optional<MapGeometryFeature> feature = lineFeatureForLineNumber(text, 5);
    if (!expect(feature.has_value(), "Expected line feature lookup through source snapshot to find the wall line.")) {
        return 1;
    }
    if (!expect(feature->lineNumber == 5 && feature->lineVertices.size() == 2,
                "Expected line feature lookup to preserve physical line numbers after blank/comment lines.")) {
        return 1;
    }
    return expect(feature->optionValues.value(QStringLiteral("id")) == QStringLiteral("wall-1"),
                  "Expected line feature lookup to preserve parsed line options.")
        ? 0
        : 1;
}
}

int main()
{
    if (const int rc = runLineFeatureLookupPreservesPhysicalLineNumbersTest(); rc != 0) {
        return rc;
    }
    return 0;
}
