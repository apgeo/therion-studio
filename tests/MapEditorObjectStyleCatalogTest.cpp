#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

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

int writeJsonFixtureFile(const QString &filePath, const char *content)
{
    QFile file(filePath);
    if (!expect(file.open(QIODevice::WriteOnly | QIODevice::Text),
                "Failed to create user style override test file.")) {
        return 1;
    }
    file.write(content);
    file.close();
    return 0;
}

int prepareUserOverrideFixture(QTemporaryDir *temporaryDir)
{
    if (!expect(temporaryDir != nullptr && temporaryDir->isValid(),
                "Expected valid temporary directory for user style overrides.")) {
        return 1;
    }

    qputenv("THERION_STUDIO_MAP_OBJECT_STYLES_DIR",
            QFile::encodeName(temporaryDir->filePath(QStringLiteral("map_object_styles"))));

    const QString overrideDirectoryPath = mapEditorUserObjectStylesDirectory();
    if (!expect(!overrideDirectoryPath.isEmpty(),
                "Expected non-empty user style override directory path.")) {
        return 1;
    }

    if (!expect(QDir().mkpath(overrideDirectoryPath),
                "Failed to create user style override test directory.")) {
        return 1;
    }

    const QDir overrideDirectory(overrideDirectoryPath);
    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("area.water.json")),
                                 R"json({
  "stroke_width": 4.25,
  "fill_opacity": 0.37
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("line.override.json")),
                                 R"json({
  "stroke_width": 5.0
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("line.override.aaa.json")),
                                 R"json({
  "stroke_width": 1.25
})json");
        result != 0) {
        return 1;
    }

    return 0;
}

int runCatalogTest()
{
    const MapEditorObjectStyleCatalog catalog = mapEditorObjectStyleCatalog();
    if (!expect(catalog.loadedFromResource,
                "Expected map object style catalog to load from resource JSON.")) {
        return 1;
    }
    if (!expect(catalog.loadedUserOverrides,
                "Expected map object style catalog to load user override JSON.")) {
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
    if (!expect(catalog.area.penStyle == Qt::DashLine,
                "Expected area default pen style to resolve as dashed.")) {
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

    const MapEditorResolvedLineStyle overrideSubtypeStyle =
        resolveMapEditorLineStyle(catalog, QStringLiteral("override"), QStringLiteral("aaa"));
    if (!expect(std::abs(overrideSubtypeStyle.strokeWidth - 1.25) < 1e-6,
                "Expected subtype user override to take precedence over type override.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle stationStyle = resolveMapEditorPointStyle(catalog, QStringLiteral("station"));
    if (!expect(stationStyle.fillColor.has_value(),
                "Expected station point fill color override.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle sandStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("sand"));
    if (!expect(sandStyle.fillPattern.has_value(),
                "Expected fill pattern for sand area style.")) {
        return 1;
    }
    if (!expect(!sandStyle.dashPattern.isEmpty(),
                "Expected stroke dash pattern for sand area style.")) {
        return 1;
    }
    if (!expect(sandStyle.fillPattern->kind == MapEditorFillPatternKind::CrossHatch,
                "Expected cross_hatch fill pattern kind for sand area style.")) {
        return 1;
    }
    if (!expect(std::abs(sandStyle.fillPattern->spacing - 7.0) < 1e-6,
                "Expected spacing override for sand area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle waterStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("water"));
    if (!expect(std::abs(waterStyle.strokeWidth - 4.25) < 1e-6,
                "Expected user override stroke width for water area style.")) {
        return 1;
    }
    if (!expect(std::abs(waterStyle.fillOpacity - 0.37) < 1e-6,
                "Expected user override fill opacity for water area style.")) {
        return 1;
    }
    if (!expect(waterStyle.fillPattern.has_value()
                    && waterStyle.fillPattern->kind == MapEditorFillPatternKind::Hatch,
                "Expected hatch fill pattern for water area style.")) {
        return 1;
    }
    if (!expect(waterStyle.fillPattern->strokeStyle == Qt::DashLine,
                "Expected dashed stroke style for water area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle blocksStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("blocks"));
    if (!expect(blocksStyle.fillPattern.has_value()
                    && blocksStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for blocks area style.")) {
        return 1;
    }
    if (!expect(std::abs(blocksStyle.fillPattern->radius - 1.1) < 1e-6,
                "Expected dot radius override for blocks area fill pattern.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Therion Studio"));
    QCoreApplication::setOrganizationName(QStringLiteral("Therion Studio"));

    QTemporaryDir temporaryDir;
    if (const int result = prepareUserOverrideFixture(&temporaryDir); result != 0) {
        return result;
    }

    return runCatalogTest();
}
