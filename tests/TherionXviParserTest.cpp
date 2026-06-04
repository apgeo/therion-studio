#include "../src/core/TherionXviParser.h"

#include <QTemporaryFile>
#include <QtMath>

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

bool nearlyEqual(qreal a, qreal b, qreal epsilon = 0.0001)
{
    return qAbs(a - b) <= epsilon;
}

int runParseTextTest()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {-10 20 5 0 0 10 4 3}\n"
        "set XVIstations {\n"
        "  {1 2 station.alpha}\n"
        "  {3 4 station.beta}\n"
        "}\n"
        "set XVIshots {\n"
        "  {0 0 10 10}\n"
        "}\n"
        "set XVIsketchlines {\n"
        "  {line 0 0 10 0 10 10}\n"
        "}\n");

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentText(xviText, &document), "Expected valid XVI text parsing to succeed.")) {
        return 1;
    }
    if (!expect(document.hasGridOrigin, "Expected parsed XVI document to include grid origin.")) {
        return 1;
    }
    if (!expect(nearlyEqual(document.gridOrigin.x(), -10.0) && nearlyEqual(document.gridOrigin.y(), 20.0),
                "Expected parsed XVI grid origin coordinates.")) {
        return 1;
    }
    if (!expect(document.hasGridDefinition, "Expected parsed XVI grid definition to be valid.")) {
        return 1;
    }
    if (!expect(document.gridCountX == 4 && document.gridCountY == 3, "Expected parsed XVI grid counts.")) {
        return 1;
    }
    if (!expect(document.stations.size() == 2, "Expected parsed XVI stations collection size.")) {
        return 1;
    }
    if (!expect(document.stationEntries.size() == 2, "Expected parsed XVI station table entry count.")) {
        return 1;
    }
    if (!expect(document.shots.size() == 1, "Expected parsed XVI shots collection size.")) {
        return 1;
    }
    if (!expect(document.sketchLines.size() == 1 && document.sketchLines.first().points.size() == 3,
                "Expected parsed XVI sketchline point count.")) {
        return 1;
    }
    if (!expect(document.sketchLines.first().colorToken == QStringLiteral("line"),
                "Expected parsed XVI sketchline color token.")) {
        return 1;
    }

    return 0;
}

int runParseFileTest()
{
    QTemporaryFile temporaryFile;
    temporaryFile.setAutoRemove(true);
    if (!expect(temporaryFile.open(), "Expected temporary file creation for XVI parser test.")) {
        return 1;
    }

    const QByteArray xviBytes =
        "set XVIgrid {0 0 1 0 0 1 2 2}\n"
        "set XVIstations {\n"
        "  {0 0 root}\n"
        "}\n"
        "set XVIshots {\n"
        "  {0 0 1 1}\n"
        "}\n";
    if (!expect(temporaryFile.write(xviBytes) == xviBytes.size(), "Expected temporary XVI test content to write fully.")) {
        return 1;
    }
    temporaryFile.flush();

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentFile(temporaryFile.fileName(), &document), "Expected valid XVI file parsing to succeed.")) {
        return 1;
    }
    if (!expect(document.hasGridOrigin && document.hasGridDefinition, "Expected parsed XVI file to include grid metadata.")) {
        return 1;
    }
    if (!expect(document.stations.contains(QStringLiteral("root")), "Expected parsed XVI file to include root station.")) {
        return 1;
    }
    if (!expect(document.shots.size() == 1, "Expected parsed XVI file to include shot geometry.")) {
        return 1;
    }

    return 0;
}

int runStationNameFieldTest()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {0 0 1 0 0 1 2 2}\n"
        "set XVIstations {\n"
        "  {1950.0348031500002 1497.5492126 1.4 trailing-token}\n"
        "}\n");

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentText(xviText, &document),
                "Expected XVI text with station trailing fields to parse.")) {
        return 1;
    }
    if (!expect(document.stations.contains(QStringLiteral("1.4")),
                "Expected XVI station name to be parsed from the third station field.")) {
        return 1;
    }
    if (!expect(document.stationEntries.size() == 1,
                "Expected XVI station table entry to be preserved.")) {
        return 1;
    }
    if (!expect(!document.stations.contains(QStringLiteral("trailing-token")),
                "Expected trailing XVI station fields not to be used as station names.")) {
        return 1;
    }
    const QPointF station = document.stations.value(QStringLiteral("1.4"));
    if (!expect(nearlyEqual(station.x(), 1950.0348031500002)
                    && nearlyEqual(station.y(), 1497.5492126),
                "Expected XVI station coordinates to be preserved when trailing fields are present.")) {
        return 1;
    }

    return 0;
}

int runDuplicateStationNameOrderTest()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {-2651.57480315 -4006.2992126 78.7401574803 0.0 0.0 78.7401574803 76.0 98.0}\n"
        "set XVIstations {\n"
        "  {-701.54 -2508.75 1.4}\n"
        "  {253.15 -2651.97 1.4}\n"
        "}\n"
        "set XVIshots {\n"
        "  {-701.54 -2508.75 253.15 -2651.97}\n"
        "}\n");

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentText(xviText, &document),
                "Expected XVI text with duplicate station names to parse.")) {
        return 1;
    }
    if (!expect(document.stationEntries.size() == 2,
                "Expected duplicate XVI station rows to be preserved in order.")) {
        return 1;
    }
    if (!expect(document.stationEntries.first().name == QStringLiteral("1.4"),
                "Expected first duplicate station row to preserve its station name.")) {
        return 1;
    }
    const QPointF firstStation = document.stationEntries.first().position;
    const QPointF secondStation = document.stationEntries.at(1).position;
    if (!expect(nearlyEqual(firstStation.x(), -701.54) && nearlyEqual(firstStation.y(), -2508.75),
                "Expected first duplicate station row coordinates to be preserved.")) {
        return 1;
    }
    if (!expect(nearlyEqual(secondStation.x(), 253.15) && nearlyEqual(secondStation.y(), -2651.97),
                "Expected second duplicate station row coordinates to be preserved.")) {
        return 1;
    }
    const QPointF lookupStation = document.stations.value(QStringLiteral("1.4"));
    if (!expect(nearlyEqual(lookupStation.x(), -701.54) && nearlyEqual(lookupStation.y(), -2508.75),
                "Expected station lookup to keep the first duplicate station, matching XTherion root placement.")) {
        return 1;
    }

    return 0;
}

int runXviGridsHeaderDoesNotMaskGridDefinitionTest()
{
    const QString xviText = QStringLiteral(
        "set XVIgrids {1.0 m}\n"
        "set XVIstations {\n"
        "  {862.52 3307.56 5.1}\n"
        "}\n"
        "set XVIgrid {219.37007874 2224.40944882 157.480314961 0.0 0.0 157.480314961 11 9}\n");

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentText(xviText, &document),
                "Expected XVI text with both XVIgrids and XVIgrid records to parse.")) {
        return 1;
    }
    if (!expect(document.hasGridOrigin && document.hasGridDefinition,
                "Expected parser to use the singular XVIgrid record as grid geometry.")) {
        return 1;
    }
    if (!expect(nearlyEqual(document.gridOrigin.x(), 219.37007874)
                    && nearlyEqual(document.gridOrigin.y(), 2224.40944882),
                "Expected parser not to treat the plural XVIgrids units record as grid geometry.")) {
        return 1;
    }
    if (!expect(document.gridCountX == 11 && document.gridCountY == 9,
                "Expected parser to preserve XVIgrid counts after an XVIgrids header.")) {
        return 1;
    }

    return 0;
}

int runRejectInvalidContentTest()
{
    const QString invalidText = QStringLiteral("set XVIgrid {0 0 1 0}\n");
    TherionXviDocument document;
    if (!expect(!parseTherionXviDocumentText(invalidText, &document),
                "Expected invalid/incomplete XVI content to be rejected.")) {
        return 1;
    }

    return 0;
}

int runSketchLineTokenPreservationTest()
{
    const QString xviText = QStringLiteral(
        "set XVIgrid {0 0 1 0 0 1 2 2}\n"
        "set XVIsketchlines {\n"
        "  {connect 0 0 1 1 2 2}\n"
        "}\n");

    TherionXviDocument document;
    if (!expect(parseTherionXviDocumentText(xviText, &document), "Expected XVI text with sketchline token to parse.")) {
        return 1;
    }
    if (!expect(document.sketchLines.size() == 1, "Expected one parsed sketchline.")) {
        return 1;
    }
    if (!expect(document.sketchLines.first().colorToken == QStringLiteral("connect"),
                "Expected sketchline token to be preserved.")) {
        return 1;
    }
    if (!expect(document.sketchLines.first().points.size() == 3, "Expected sketchline point count to remain preserved.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (const int rc = runParseTextTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runParseFileTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runStationNameFieldTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runDuplicateStationNameOrderTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runXviGridsHeaderDoesNotMaskGridDefinitionTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runRejectInvalidContentTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSketchLineTokenPreservationTest(); rc != 0) {
        return rc;
    }

    return 0;
}
