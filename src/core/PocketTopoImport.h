#pragma once

#include <QString>

namespace TherionStudio
{
enum class PocketTopoProjection
{
    Plan,
    Elevation
};

struct PocketTopoXviImportOptions
{
    int scale = 200;
    int resolutionDpi = 200;
    double gridSpacingMeters = 1.0;
    PocketTopoProjection projection = PocketTopoProjection::Plan;
};

QString convertPocketTopoTextToTherionCentreline(const QString &content);
QString convertPocketTopoTextToXvi(const QString &content,
                                   const PocketTopoXviImportOptions &options,
                                   bool *hasProjectedData = nullptr);
QString pocketTopoProjectionSuffix(PocketTopoProjection projection);
}
