#include "../src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.h"
#include "../src/core/TherionDocumentParser.h"

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

QJsonObject orientationOption(const QJsonArray &applicableTypes, const QJsonArray &excludedTypes = QJsonArray())
{
    QJsonObject option;
    option.insert(QStringLiteral("option_key"), QStringLiteral("-orientation"));
    option.insert(QStringLiteral("applicable_types"), applicableTypes);
    option.insert(QStringLiteral("excluded_types"), excludedTypes);
    return option;
}

int runInjectedOrientationApplicabilityTest()
{
    QJsonObject pointCommand;
    pointCommand.insert(QStringLiteral("name"), QStringLiteral("point"));
    pointCommand.insert(QStringLiteral("options"), QJsonArray({orientationOption(QJsonArray({QStringLiteral("station")}))}));

    QJsonObject lineCommand;
    lineCommand.insert(QStringLiteral("name"), QStringLiteral("line"));
    lineCommand.insert(QStringLiteral("options"), QJsonArray({orientationOption(QJsonArray(), QJsonArray({QStringLiteral("wall")}))}));

    QJsonObject catalogObject;
    catalogObject.insert(QStringLiteral("commands"), QJsonArray({pointCommand, lineCommand}));

    const MapEditorOrientationApplicabilityByCommand applicability =
        mapEditorOrientationApplicabilityFromCommandCatalog(catalogObject);

    const TherionParsedLine stationPoint = TherionDocumentParser::parseLine(QStringLiteral("point 1 2 station"), 1);
    if (!expect(isOrientationSupportedForParsedLine(stationPoint, applicability),
                "Injected orientation catalog should allow listed point types.")) {
        return 1;
    }

    const TherionParsedLine labelPoint = TherionDocumentParser::parseLine(QStringLiteral("point 1 2 label"), 1);
    if (!expect(!isOrientationSupportedForParsedLine(labelPoint, applicability),
                "Injected orientation catalog should reject point types outside applicable_types.")) {
        return 1;
    }

    const TherionParsedLine slopeLine = TherionDocumentParser::parseLine(QStringLiteral("line slope"), 1);
    if (!expect(isOrientationSupportedForParsedLine(slopeLine, applicability),
                "Injected orientation catalog should allow line types not listed in excluded_types.")) {
        return 1;
    }

    const TherionParsedLine wallLine = TherionDocumentParser::parseLine(QStringLiteral("line wall"), 1);
    if (!expect(!isOrientationSupportedForParsedLine(wallLine, applicability),
                "Injected orientation catalog should reject excluded line types.")) {
        return 1;
    }

    return 0;
}

int runObjectCommandCatalogShapeTest()
{
    QJsonObject pointOption;
    pointOption.insert(QStringLiteral("option_key"), QStringLiteral("-orientation"));
    pointOption.insert(QStringLiteral("description"), QStringLiteral("Allowed only with station type."));
    QJsonObject pointCommand;
    pointCommand.insert(QStringLiteral("options"), QJsonArray({pointOption}));

    QJsonObject commandsObject;
    commandsObject.insert(QStringLiteral("point"), pointCommand);
    QJsonObject catalogObject;
    catalogObject.insert(QStringLiteral("commands"), commandsObject);

    const MapEditorOrientationApplicabilityByCommand applicability =
        mapEditorOrientationApplicabilityFromCommandCatalog(catalogObject);

    const TherionParsedLine stationPoint = TherionDocumentParser::parseLine(QStringLiteral("point 1 2 station"), 1);
    if (!expect(isOrientationSupportedForParsedLine(stationPoint, applicability),
                "Object-shaped command catalogs should be accepted for orientation metadata.")) {
        return 1;
    }

    const TherionParsedLine labelPoint = TherionDocumentParser::parseLine(QStringLiteral("point 1 2 label"), 1);
    if (!expect(!isOrientationSupportedForParsedLine(labelPoint, applicability),
                "Description-derived applicable type metadata should restrict object-shaped catalogs.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int result = runInjectedOrientationApplicabilityTest(); result != 0) {
        return result;
    }
    return runObjectCommandCatalogShapeTest();
}
