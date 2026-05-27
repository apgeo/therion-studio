#include "../src/app/text_editor/map_editor/MapEditorObjectStyleCatalog.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPair>
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

int writeJsonFixtureFile(const QString &filePath, const QByteArray &content)
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

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("point.override.json")),
                                 R"json({
  "symbol": "x",
  "size": 13.0,
  "label_field": "-id"
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("area.symbols.json")),
                                 R"json({
  "fill_pattern": {
    "kind": "dots",
    "symbol": "oval",
    "size": 5.5,
    "size_jitter": 1.25,
    "dot_color": "#223344",
    "angle_jitter": 17.5
  }
})json");
        result != 0) {
        return 1;
    }

    if (const int result =
            writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("point.oval.json")),
                                 R"json({
  "symbol": "oval",
  "size": 14.0
})json");
        result != 0) {
        return 1;
    }

    const QVector<QPair<QString, QString>> pointSymbolFiles = {
        {QStringLiteral("circle"), QStringLiteral("circle")},
        {QStringLiteral("square"), QStringLiteral("square")},
        {QStringLiteral("diamond"), QStringLiteral("diamond")},
        {QStringLiteral("triangle"), QStringLiteral("triangle")},
        {QStringLiteral("star"), QStringLiteral("star")},
        {QStringLiteral("asterisk"), QStringLiteral("asterisk")},
        {QStringLiteral("plus"), QStringLiteral("plus")},
        {QStringLiteral("x"), QStringLiteral("x")}
    };
    for (const QPair<QString, QString> &entry : pointSymbolFiles) {
        const QByteArray content = QByteArray("{\n  \"symbol\": \"")
            + entry.second.toUtf8()
            + QByteArray("\"\n}\n");
        if (const int result =
                writeJsonFixtureFile(overrideDirectory.filePath(QStringLiteral("point.%1.json").arg(entry.first)),
                                     content);
            result != 0) {
            return 1;
        }
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

    if (!expect(catalog.point.symbol == MapEditorPointSymbol::Circle,
                "Expected point default symbol from resource catalog.")) {
        return 1;
    }
    if (!expect(std::abs(catalog.point.size - 11.2) < 1e-6,
                "Expected point default size from resource catalog.")) {
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
    if (!expect(stationStyle.symbol == MapEditorPointSymbol::Triangle,
                "Expected station point symbol override.")) {
        return 1;
    }
    if (!expect(stationStyle.labelField.has_value()
                    && stationStyle.labelField.value() == QStringLiteral("name"),
                "Expected station point label field override.")) {
        return 1;
    }
    if (!expect(std::abs(stationStyle.size - 8.4) < 1e-6,
                "Expected station point size override.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle labelStyle = resolveMapEditorPointStyle(catalog, QStringLiteral("label"));
    if (!expect(labelStyle.labelField.has_value()
                    && labelStyle.labelField.value() == QStringLiteral("text"),
                "Expected label point label field override.")) {
        return 1;
    }

    const MapEditorResolvedPointStyle overridePointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("override"));
    if (!expect(overridePointStyle.symbol == MapEditorPointSymbol::X,
                "Expected user override point symbol.")) {
        return 1;
    }
    if (!expect(std::abs(overridePointStyle.size - 13.0) < 1e-6,
                "Expected user override point size.")) {
        return 1;
    }
    if (!expect(overridePointStyle.labelField.has_value()
                    && overridePointStyle.labelField.value() == QStringLiteral("id"),
                "Expected user override point label field to normalize without leading dash.")) {
        return 1;
    }

    const QVector<QPair<QString, MapEditorPointSymbol>> symbolCases = {
        {QStringLiteral("circle"), MapEditorPointSymbol::Circle},
        {QStringLiteral("square"), MapEditorPointSymbol::Square},
        {QStringLiteral("diamond"), MapEditorPointSymbol::Diamond},
        {QStringLiteral("triangle"), MapEditorPointSymbol::Triangle},
        {QStringLiteral("star"), MapEditorPointSymbol::Star},
        {QStringLiteral("asterisk"), MapEditorPointSymbol::Asterisk},
        {QStringLiteral("plus"), MapEditorPointSymbol::Plus},
        {QStringLiteral("x"), MapEditorPointSymbol::X}
    };
    for (const QPair<QString, MapEditorPointSymbol> &symbolCase : symbolCases) {
        const MapEditorResolvedPointStyle symbolStyle = resolveMapEditorPointStyle(catalog, symbolCase.first);
        if (!expect(symbolStyle.symbol == symbolCase.second,
                    "Expected supported point symbol style to resolve from user override.")) {
            return 1;
        }
    }
    const MapEditorResolvedPointStyle ovalPointStyle =
        resolveMapEditorPointStyle(catalog, QStringLiteral("oval"));
    if (!expect(ovalPointStyle.symbol == MapEditorPointSymbol::Circle,
                "Expected oval symbol to stay unsupported for point styles.")) {
        return 1;
    }
    if (!expect(std::abs(ovalPointStyle.size - 14.0) < 1e-6,
                "Expected non-symbol point style fields to keep loading for oval point fixture.")) {
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
    if (!expect(sandStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern kind for sand area style.")) {
        return 1;
    }
    if (!expect(std::abs(sandStyle.fillPattern->spacing - 6.4) < 1e-6,
                "Expected spacing override for sand area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle clayStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("clay"));
    if (!expect(clayStyle.fillPattern.has_value()
                    && clayStyle.fillPattern->kind == MapEditorFillPatternKind::Hatch,
                "Expected hatch fill pattern for clay area style.")) {
        return 1;
    }
    if (!expect(clayStyle.fillPattern->angleJitter > 0.0,
                "Expected angle jitter for clay area fill pattern.")) {
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
    if (!expect(std::abs(waterStyle.fillPattern->angle) < 1e-6,
                "Expected horizontal hatch angle for water area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle sumpStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("sump"));
    if (!expect(sumpStyle.fillPattern.has_value()
                    && sumpStyle.fillPattern->kind == MapEditorFillPatternKind::Hatch,
                "Expected hatch fill pattern for sump area style.")) {
        return 1;
    }
    if (!expect(sumpStyle.fillPattern->strokeStyle == Qt::SolidLine,
                "Expected solid stroke style for sump area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle iceStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("ice"));
    if (!expect(iceStyle.fillPattern.has_value()
                    && iceStyle.fillPattern->kind == MapEditorFillPatternKind::CrossHatch,
                "Expected cross_hatch fill pattern for ice area style.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle mudcrackStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("mudcrack"));
    if (!expect(mudcrackStyle.fillPattern.has_value()
                    && mudcrackStyle.fillPattern->kind == MapEditorFillPatternKind::CrossHatch,
                "Expected cross_hatch fill pattern for mudcrack area style.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle blocksStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("blocks"));
    if (!expect(blocksStyle.fillPattern.has_value()
                    && blocksStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for blocks area style.")) {
        return 1;
    }
    if (!expect(blocksStyle.fillPattern->symbol == MapEditorPointSymbol::Square,
                "Expected square dot symbol for blocks area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(blocksStyle.fillPattern->size - 3.0) < 1e-6,
                "Expected dot symbol size override for blocks area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle pebblesStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("pebbles"));
    if (!expect(pebblesStyle.fillPattern.has_value()
                    && pebblesStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for pebbles area style.")) {
        return 1;
    }
    if (!expect(pebblesStyle.fillPattern->symbol == MapEditorPointSymbol::Oval,
                "Expected oval dot symbol for pebbles area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle debrisStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("debris"));
    if (!expect(debrisStyle.fillPattern.has_value()
                    && debrisStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for debris area style.")) {
        return 1;
    }
    if (!expect(debrisStyle.fillPattern->symbol == MapEditorPointSymbol::Triangle,
                "Expected triangle dot symbol for debris area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle snowStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("snow"));
    if (!expect(snowStyle.fillPattern.has_value()
                    && snowStyle.fillPattern->kind == MapEditorFillPatternKind::Dots,
                "Expected dots fill pattern for snow area style.")) {
        return 1;
    }
    if (!expect(snowStyle.fillPattern->symbol == MapEditorPointSymbol::Asterisk,
                "Expected asterisk symbol for snow area fill pattern.")) {
        return 1;
    }

    const MapEditorResolvedAreaStyle symbolStyle = resolveMapEditorAreaStyle(catalog, QStringLiteral("symbols"));
    if (!expect(symbolStyle.fillPattern.has_value()
                    && symbolStyle.fillPattern->symbol == MapEditorPointSymbol::Oval,
                "Expected user override dot symbol for area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(symbolStyle.fillPattern->size - 5.5) < 1e-6,
                "Expected user override dot symbol size for area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(symbolStyle.fillPattern->sizeJitter - 1.25) < 1e-6,
                "Expected user override dot symbol size jitter for area fill pattern.")) {
        return 1;
    }
    if (!expect(std::abs(symbolStyle.fillPattern->angleJitter - 17.5) < 1e-6,
                "Expected user override dot symbol angle jitter for area fill pattern.")) {
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
