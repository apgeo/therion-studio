#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"

#include <cmath>
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
}

int main()
{
    const MapEditorObjectStyleCatalog catalog = mapEditorObjectStyleCatalog();
    if (!expect(catalog.loadedFromResource,
                "Expected map object style catalog to load from resource JSON.")) {
        return 1;
    }

    if (!expect(std::abs(catalog.point.radius - 5.6) < 1e-6,
                "Expected point default radius from resource catalog.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.line.strokeWidth - 3.2) < 1e-6,
                "Expected line default stroke width from resource catalog.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.line.detailWidth - 1.8) < 1e-6,
                "Expected line default detail width from resource catalog.")) {
        return 1;
    }
    if (!expect(catalog.line.penStyle == Qt::SolidLine,
                "Expected line default pen style to resolve as solid.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.area.strokeWidth - 2.0) < 1e-6,
                "Expected area default stroke width from resource catalog.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.area.fillOpacity - 0.11) < 1e-6,
                "Expected area default fill opacity from resource catalog.")) {
        return 1;
    }
    if (!expect(catalog.area.penStyle == Qt::SolidLine,
                "Expected area default pen style to resolve as solid.")) {
        return 1;
    }
    if (!expect(!catalog.lineStyles.isEmpty(),
                "Expected type-specific line style rules to load from resource catalog.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle wallStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("wall"));
    if (!expect(std::abs(wallStyle.strokeWidth - 2.6) < 1e-6,
                "Expected line style override width for wall.")) {
        return 1;
    }

    const MapEditorResolvedLineStyle borderStyle = resolveMapEditorLineStyle(catalog, QStringLiteral("border"));
    if (!expect(!borderStyle.dashPattern.isEmpty(),
                "Expected dash pattern override for border line style.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle stationStyle = resolveMapEditorPointStyle(catalog, QStringLiteral("station"));
    if (!expect(stationStyle.fillColor.has_value(),
                "Expected station point fill color override.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle sandStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("sand"));
    if (!expect(sandStyle.useFillPattern,
                "Expected fill pattern for sand area style.")) {
        return 1;
    }
    if (!expect(!sandStyle.dashPattern.isEmpty(),
                "Expected stroke dash pattern for sand area style.")) {
        return 1;
    }

    return 0;
}
