#include "../src/app/text_editor/map_editor/MapEditorObjectDetailsLogic.h"
#include "../src/core/TherionDocumentParser.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

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

bool expectListEquals(const QStringList &actual, const QStringList &expected, const char *message)
{
    if (actual == expected) {
        return true;
    }
    std::cerr << message << '\n';
    std::cerr << "  expected:";
    for (const QString &row : expected) {
        std::cerr << " [" << row.toStdString() << "]";
    }
    std::cerr << "\n  actual:";
    for (const QString &row : actual) {
        std::cerr << " [" << row.toStdString() << "]";
    }
    std::cerr << '\n';
    return false;
}

int runLinePointStructuredStandaloneRowsTest()
{
    const QStringList rows{
        QStringLiteral("subtype blocks"),
        QStringLiteral("altitude ."),
        QStringLiteral("adjust horizontal"),
        QStringLiteral("altitude [fix 1510 m]"),
    };

    if (!expect(linePointSegmentSubtypeFromStandaloneRows(rows) == QStringLiteral("blocks"),
                "Line-point subtype should be read from standalone subtype rows.")) {
        return 1;
    }
    if (!expect(linePointAltitudeAutoFromStandaloneRows(rows),
                "Line-point altitude auto should be detected from altitude dot rows.")) {
        return 1;
    }
    if (!expect(linePointRowsShouldHighlightVertexMetadata(
                    QStringList{QStringLiteral("altitude [fix 1510 m]")}),
                "Line-point altitude rows should highlight the map vertex.")) {
        return 1;
    }
    if (!expect(linePointRowsShouldHighlightVertexMetadata(QStringList{QStringLiteral("-subtype temporary")}),
                "Line-point subtype rows should highlight the map vertex.")) {
        return 1;
    }
    if (!expect(!linePointRowsShouldHighlightVertexMetadata(
                    QStringList{QStringLiteral("smooth on"),
                                QStringLiteral("-orientation 45"),
                                QStringLiteral("l-size 40"),
                                QStringLiteral("direction begin"),
                                QStringLiteral("adjust horizontal")}),
                "Line-point smooth/orientation/l-size/direction/adjust rows should not highlight the map vertex.")) {
        return 1;
    }
    if (!expectListEquals(linePointRowsWithoutStructuredStandaloneOptions(rows, true, true),
                          QStringList{QStringLiteral("adjust horizontal"),
                                      QStringLiteral("altitude [fix 1510 m]")},
                          "Managed line-point rows should remove only structured subtype and altitude dot rows.")) {
        return 1;
    }
    if (!expectListEquals(linePointRowsWithoutStructuredStandaloneOptions(rows, false, false),
                          rows,
                          "Unsupported structured line-point rows should remain in the manual editor.")) {
        return 1;
    }
    if (!expectListEquals(linePointRowsWithStructuredStandaloneOptions(QStringList{QStringLiteral("adjust horizontal")},
                                                                       QStringLiteral("presumed"),
                                                                       true),
                          QStringList{QStringLiteral("adjust horizontal"),
                                      QStringLiteral("subtype presumed"),
                                      QStringLiteral("altitude .")},
                          "Structured line-point rows should be appended to the manual rows.")) {
        return 1;
    }
    if (!expectListEquals(linePointRowsWithStructuredStandaloneOptions(QStringList{QStringLiteral("subtype blocks"),
                                                                                  QStringLiteral("altitude ."),
                                                                                  QStringLiteral("mark start")},
                                                                       QString(),
                                                                       false),
                          QStringList{QStringLiteral("mark start")},
                          "Cleared structured line-point controls should remove their managed rows.")) {
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
    if (const int result = runObjectCommandCatalogShapeTest(); result != 0) {
        return result;
    }
    return runLinePointStructuredStandaloneRowsTest();
}
