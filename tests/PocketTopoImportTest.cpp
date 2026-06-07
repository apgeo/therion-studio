#include "../src/core/PocketTopoImport.h"
#include "../src/core/TherionXviParser.h"

#include <QCoreApplication>

#include <iostream>

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

QString samplePocketTopoText()
{
    return QStringLiteral(
        "FIX\n"
        "A1 10 20 30\n"
        "TRIP\n"
        "DATE 2026-06-07\n"
        "DECLINATION 1.5\n"
        "DATA\n"
        "1 2 12.3 45.0 3.0 >\n"
        "2 3 5.0 90.0 -1.0 >\n"
        "3 4 4.0 120.0 0.5 <\n"
        "PLAN\n"
        "STATIONS\n"
        "0 0 1\n"
        "1 0 2\n"
        "SHOTS\n"
        "0 0 1 0\n"
        "POLYLINE RED\n"
        "0 0\n"
        "1 1\n"
        "ELEVATION\n"
        "STATIONS\n"
        "0 0 1\n");
}

bool testCentrelineImport()
{
    const QString result = TherionStudio::convertPocketTopoTextToTherionCentreline(samplePocketTopoText());
    return expect(result.contains(QStringLiteral("centreline\n  fix A1 10 20 30\nendcentreline")),
                  "Expected FIX section to become a centreline with fix rows.")
        && expect(result.contains(QStringLiteral("  date 2026.06.07\n")),
                  "Expected DATE to use Therion dot date format.")
        && expect(result.contains(QStringLiteral("  data normal from to compass clino tape\n")),
                  "Expected DATA to become a Therion normal data declaration.")
        && expect(result.contains(QStringLiteral("  extend right\n  1 2 12.3 45.0 3.0\n")),
                  "Expected right extend marker before first right-facing shot.")
        && expect(result.contains(QStringLiteral("  extend left\n  3 4 4.0 120.0 0.5\n")),
                  "Expected left extend marker when direction changes.");
}

bool testXviImport()
{
    TherionStudio::PocketTopoXviImportOptions options;
    options.scale = 200;
    options.resolutionDpi = 200;
    options.gridSpacingMeters = 1.0;
    options.projection = TherionStudio::PocketTopoProjection::Plan;

    bool hasData = false;
    const QString result = TherionStudio::convertPocketTopoTextToXvi(samplePocketTopoText(), options, &hasData);
    TherionStudio::TherionXviDocument document;
    return expect(hasData, "Expected PLAN projection data to be detected.")
        && expect(result.contains(QStringLiteral("set XVIgrids {1 m}")),
                  "Expected XVI grid spacing units line.")
        && expect(TherionStudio::parseTherionXviDocumentText(result, &document),
                  "Expected generated XVI text to parse.")
        && expect(document.hasGridDefinition, "Expected generated XVI to include grid geometry.")
        && expect(document.stationEntries.size() == 2, "Expected two PLAN stations.")
        && expect(document.shots.size() == 1, "Expected one PLAN shot.")
        && expect(document.sketchLines.size() == 1, "Expected one PLAN sketch polyline.")
        && expect(document.sketchLines.first().colorToken == QStringLiteral("red"),
                  "Expected polyline color token to be lower-cased like XTherion.")
        && expect(document.sketchLines.first().points.size() == 2,
                  "Expected two sketch polyline points.");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    ok = testCentrelineImport() && ok;
    ok = testXviImport() && ok;
    return ok ? 0 : 1;
}
