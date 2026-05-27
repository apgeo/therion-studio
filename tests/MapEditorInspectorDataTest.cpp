#include "../src/app/text_editor/map_editor/MapEditorInspectorData.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/TherionDocumentParser.h"

#include <QComboBox>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
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

int runInspectorTypeCatalogLoadTest()
{
    const CommandCatalogStore catalogStore;
    const InspectorSymbolCatalog catalog = inspectorSymbolCatalogFromCommandCatalog(catalogStore.catalogObject());

    const QStringList pointTypes = inspectorTypeValuesForCommand(catalog, QStringLiteral("point"));
    if (!expect(!pointTypes.isEmpty(), "Point type values should not be empty.")) {
        return 1;
    }
    if (!expect(pointTypes.contains(QStringLiteral("station")),
                "Point type values should include 'station'.")) {
        return 1;
    }

    const QStringList lineTypes = inspectorTypeValuesForCommand(catalog, QStringLiteral("line"));
    if (!expect(!lineTypes.isEmpty(), "Line type values should not be empty.")) {
        return 1;
    }
    if (!expect(lineTypes.contains(QStringLiteral("wall")),
                "Line type values should include 'wall'.")) {
        return 1;
    }

    const QStringList areaTypes = inspectorTypeValuesForCommand(catalog, QStringLiteral("area"));
    if (!expect(!areaTypes.isEmpty(), "Area type values should not be empty.")) {
        return 1;
    }
    if (!expect(areaTypes.contains(QStringLiteral("water")),
                "Area type values should include 'water'.")) {
        return 1;
    }

    return 0;
}

int runAreaQuickTypeComboPopulationTest()
{
    const CommandCatalogStore catalogStore;
    const InspectorSymbolCatalog catalog = inspectorSymbolCatalogFromCommandCatalog(catalogStore.catalogObject());

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(QStringLiteral("area water"), 1);
    const std::optional<InspectorObjectQuickFields> fields = inspectorObjectQuickFieldsFromParsedLine(parsedLine);
    if (!expect(fields.has_value(), "Area quick fields should be extracted from parsed area line.")) {
        return 1;
    }
    if (!expect(fields->commandKind == QStringLiteral("area") && fields->type == QStringLiteral("water"),
                "Area quick fields should preserve command kind and parsed type.")) {
        return 1;
    }

    QComboBox combo;
    combo.setEditable(true);
    combo.setInsertPolicy(QComboBox::NoInsert);
    setEditableComboValues(&combo, inspectorTypeValuesForCommand(catalog, fields->commandKind), fields->type);

    if (!expect(combo.count() > 1,
                "Area type combo should contain catalog values, not only the parsed type.")) {
        return 1;
    }
    if (!expect(combo.currentText() == QStringLiteral("water"),
                "Area type combo should keep parsed type as current text.")) {
        return 1;
    }
    if (!expect(combo.findText(QStringLiteral("sand")) >= 0,
                "Area type combo should include alternate catalog values such as sand.")) {
        return 1;
    }

    return 0;
}

int runInjectedCatalogTest()
{
    QJsonObject pointCommand;
    pointCommand.insert(QStringLiteral("name"), QStringLiteral("point"));
    pointCommand.insert(QStringLiteral("type_values"), QJsonArray({QStringLiteral("custom-point")}));
    QJsonObject pointSubtypes;
    pointSubtypes.insert(QStringLiteral("custom-point"), QJsonArray({QStringLiteral("custom-subtype")}));
    pointCommand.insert(QStringLiteral("subtype_by_type"), pointSubtypes);

    QJsonObject scrapProjectionOption;
    scrapProjectionOption.insert(QStringLiteral("option_key"), QStringLiteral("-projection"));
    scrapProjectionOption.insert(QStringLiteral("allowed_values"), QJsonArray({QStringLiteral("custom-projection")}));
    QJsonObject scrapCommand;
    scrapCommand.insert(QStringLiteral("name"), QStringLiteral("scrap"));
    scrapCommand.insert(QStringLiteral("options"), QJsonArray({scrapProjectionOption}));

    QJsonObject catalogObject;
    catalogObject.insert(QStringLiteral("commands"), QJsonArray({pointCommand, scrapCommand}));

    const InspectorSymbolCatalog catalog = inspectorSymbolCatalogFromCommandCatalog(catalogObject);
    if (!expect(inspectorTypeValuesForCommand(catalog, QStringLiteral("point")).contains(QStringLiteral("custom-point")),
                "Injected inspector catalog should provide point type values without resource loading.")) {
        return 1;
    }
    if (!expect(inspectorSubtypeValuesForCommandType(catalog,
                                                     QStringLiteral("point"),
                                                     QStringLiteral("custom-point"))
                    .contains(QStringLiteral("custom-subtype")),
                "Injected inspector catalog should provide subtype values without resource loading.")) {
        return 1;
    }
    if (!expect(inspectorProjectionValues(catalog).contains(QStringLiteral("custom-projection")),
                "Injected inspector catalog should provide projection values without resource loading.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int result = runInspectorTypeCatalogLoadTest(); result != 0) {
        return result;
    }
    if (const int result = runInjectedCatalogTest(); result != 0) {
        return result;
    }
    return runAreaQuickTypeComboPopulationTest();
}
