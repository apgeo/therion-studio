#include "PocketTopoImport.h"

#include "TherionStringUtils.h"

#include <QPointF>
#include <QtGlobal>

#include <cmath>

namespace TherionStudio
{
namespace
{
enum class PocketTopoSection
{
    None,
    Fix,
    Trip,
    Plan,
    Elevation
};

struct PocketTopoXviBounds
{
    bool defined = false;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
};

struct PocketTopoPolyline
{
    QString colorToken;
    QVector<QPointF> points;
};

PocketTopoSection sectionFromLine(const QString &line)
{
    if (line == QStringLiteral("FIX")) {
        return PocketTopoSection::Fix;
    }
    if (line == QStringLiteral("TRIP")) {
        return PocketTopoSection::Trip;
    }
    if (line == QStringLiteral("PLAN")) {
        return PocketTopoSection::Plan;
    }
    if (line == QStringLiteral("ELEVATION")) {
        return PocketTopoSection::Elevation;
    }
    return PocketTopoSection::None;
}

QString projectionSectionName(PocketTopoProjection projection)
{
    return projection == PocketTopoProjection::Elevation
        ? QStringLiteral("ELEVATION")
        : QStringLiteral("PLAN");
}

QString formatPocketTopoXviNumber(double value)
{
    if (std::fabs(value) < 1e-9) {
        value = 0.0;
    }
    return QString::number(value, 'g', 15);
}

QString formatPocketTopoCoordinate(double value)
{
    return QString::number(value, 'f', 2);
}

bool parseDoubleToken(const QString &token, double *value)
{
    if (value == nullptr) {
        return false;
    }

    bool ok = false;
    const double parsed = token.toDouble(&ok);
    if (!ok) {
        return false;
    }
    *value = parsed;
    return true;
}

void includeBounds(PocketTopoXviBounds *bounds, const QPointF &point)
{
    if (bounds == nullptr) {
        return;
    }
    if (!bounds->defined) {
        bounds->defined = true;
        bounds->minX = point.x();
        bounds->maxX = point.x();
        bounds->minY = point.y();
        bounds->maxY = point.y();
        return;
    }

    bounds->minX = qMin(bounds->minX, point.x());
    bounds->maxX = qMax(bounds->maxX, point.x());
    bounds->minY = qMin(bounds->minY, point.y());
    bounds->maxY = qMax(bounds->maxY, point.y());
}

bool transformPocketTopoPoint(const QString &xToken,
                              const QString &yToken,
                              double scaleFactor,
                              PocketTopoXviBounds *bounds,
                              QPointF *point)
{
    if (point == nullptr) {
        return false;
    }

    double x = 0.0;
    double y = 0.0;
    if (!parseDoubleToken(xToken, &x) || !parseDoubleToken(yToken, &y)) {
        return false;
    }

    *point = QPointF(x * scaleFactor, y * scaleFactor);
    includeBounds(bounds, *point);
    return true;
}

void flushCentrelineSection(QStringList *sections, QString *currentSectionText)
{
    if (sections == nullptr || currentSectionText == nullptr || currentSectionText->isEmpty()) {
        return;
    }

    sections->append(QStringLiteral("centreline\n%1endcentreline\n").arg(*currentSectionText));
    currentSectionText->clear();
}
}

QString pocketTopoProjectionSuffix(PocketTopoProjection projection)
{
    return projection == PocketTopoProjection::Elevation ? QStringLiteral("e") : QStringLiteral("p");
}

QString convertPocketTopoTextToTherionCentreline(const QString &content)
{
    const QStringList lines = splitLinesTrimmingCarriageReturns(content);
    QStringList centrelineSections;
    QString currentSectionText;
    PocketTopoSection currentSection = PocketTopoSection::None;
    bool inTripData = false;
    int currentExtendDirection = 0;

    for (QString line : lines) {
        line = line.trimmed();
        const PocketTopoSection nextSection = sectionFromLine(line);
        if (nextSection != PocketTopoSection::None) {
            flushCentrelineSection(&centrelineSections, &currentSectionText);
            currentSection = nextSection;
            inTripData = false;
            currentExtendDirection = 0;
        }

        const QStringList tokens = tokenizeWhitespace(line);
        if (tokens.isEmpty()) {
            continue;
        }

        if (currentSection == PocketTopoSection::Fix) {
            if (tokens.size() == 4) {
                currentSectionText += QStringLiteral("  fix %1 %2 %3 %4\n")
                                          .arg(tokens.at(0), tokens.at(1), tokens.at(2), tokens.at(3));
            }
            continue;
        }

        if (currentSection != PocketTopoSection::Trip) {
            continue;
        }

        if (inTripData) {
            const QString lastToken = tokens.last();
            if (lastToken == QStringLiteral("<")) {
                if (currentExtendDirection >= 0) {
                    currentSectionText += QStringLiteral("  extend left\n");
                    currentExtendDirection = -1;
                }
            } else if (lastToken == QStringLiteral(">")) {
                if (currentExtendDirection <= 0) {
                    currentSectionText += QStringLiteral("  extend right\n");
                    currentExtendDirection = 1;
                }
            }

            if (tokens.size() == 5) {
                currentSectionText += QStringLiteral("  %1 - %2 %3 %4\n")
                                          .arg(tokens.at(0), tokens.at(1), tokens.at(2), tokens.at(3));
            } else if (tokens.size() == 6) {
                currentSectionText += QStringLiteral("  %1 %2 %3 %4 %5\n")
                                          .arg(tokens.at(0), tokens.at(1), tokens.at(2), tokens.at(3), tokens.at(4));
            }
            continue;
        }

        if (tokens.first() == QStringLiteral("DATE") && tokens.size() >= 2) {
            QString dateToken = tokens.at(1);
            dateToken.replace(QLatin1Char('-'), QLatin1Char('.'));
            currentSectionText += QStringLiteral("  date %1\n").arg(dateToken);
        } else if (tokens.first() == QStringLiteral("DATA")) {
            currentSectionText += QStringLiteral("  data normal from to compass clino tape\n");
            inTripData = true;
        }
    }

    flushCentrelineSection(&centrelineSections, &currentSectionText);
    return centrelineSections.join(QLatin1Char('\n'));
}

QString convertPocketTopoTextToXvi(const QString &content,
                                   const PocketTopoXviImportOptions &options,
                                   bool *hasProjectedData)
{
    if (hasProjectedData != nullptr) {
        *hasProjectedData = false;
    }

    const int scale = options.scale > 0 ? options.scale : 200;
    const int resolutionDpi = options.resolutionDpi > 0 ? options.resolutionDpi : 200;
    const double gridSpacingMeters = options.gridSpacingMeters > 0.0 ? options.gridSpacingMeters : 1.0;
    const double scaleFactor = (1.0 / static_cast<double>(scale)) * (100.0 / 2.54) * static_cast<double>(resolutionDpi);
    const QString targetSectionName = projectionSectionName(options.projection);

    bool inTargetProjection = false;
    bool inStations = false;
    bool inShots = false;
    bool inPolyline = false;
    PocketTopoXviBounds bounds;
    QStringList stationRows;
    QStringList shotRows;
    QVector<PocketTopoPolyline> polylines;

    const QStringList lines = splitLinesTrimmingCarriageReturns(content);
    for (QString line : lines) {
        line = line.trimmed();
        if (line == targetSectionName) {
            inTargetProjection = true;
            inStations = false;
            inShots = false;
            inPolyline = false;
            continue;
        }
        if (line == QStringLiteral("PLAN") || line == QStringLiteral("ELEVATION")) {
            inTargetProjection = false;
            inStations = false;
            inShots = false;
            inPolyline = false;
            continue;
        }
        if (!inTargetProjection) {
            continue;
        }

        const QStringList tokens = tokenizeWhitespace(line);
        if (tokens.isEmpty()) {
            continue;
        }

        if (tokens.first() == QStringLiteral("STATIONS")) {
            inStations = true;
            inShots = false;
            inPolyline = false;
            continue;
        }
        if (tokens.first() == QStringLiteral("SHOTS")) {
            inStations = false;
            inShots = true;
            inPolyline = false;
            continue;
        }
        if (tokens.first() == QStringLiteral("POLYLINE")) {
            inStations = false;
            inShots = false;
            inPolyline = true;
            PocketTopoPolyline polyline;
            polyline.colorToken = tokens.size() >= 2 ? tokens.at(1).toLower() : QStringLiteral("black");
            polylines.append(polyline);
            continue;
        }

        QPointF point;
        if (inStations && tokens.size() == 3) {
            if (transformPocketTopoPoint(tokens.at(0), tokens.at(1), scaleFactor, &bounds, &point)) {
                stationRows.append(QStringLiteral("  {%1 %2 %3}")
                                       .arg(formatPocketTopoCoordinate(point.x()),
                                            formatPocketTopoCoordinate(point.y()),
                                            tokens.at(2)));
            }
            continue;
        }

        if (inShots && tokens.size() == 4) {
            QPointF from;
            QPointF to;
            if (transformPocketTopoPoint(tokens.at(0), tokens.at(1), scaleFactor, &bounds, &from)
                && transformPocketTopoPoint(tokens.at(2), tokens.at(3), scaleFactor, &bounds, &to)) {
                shotRows.append(QStringLiteral("  {%1 %2 %3 %4}")
                                    .arg(formatPocketTopoCoordinate(from.x()),
                                         formatPocketTopoCoordinate(from.y()),
                                         formatPocketTopoCoordinate(to.x()),
                                         formatPocketTopoCoordinate(to.y())));
            }
            continue;
        }

        if (inPolyline && tokens.size() == 2 && !polylines.isEmpty()) {
            if (transformPocketTopoPoint(tokens.at(0), tokens.at(1), scaleFactor, &bounds, &point)) {
                polylines.last().points.append(point);
            }
        }
    }

    if (!bounds.defined) {
        return QString();
    }
    if (hasProjectedData != nullptr) {
        *hasProjectedData = true;
    }

    const double gridSize = gridSpacingMeters * scaleFactor;
    const double gridMinX = bounds.minX - (0.5 * gridSize);
    const double gridMinY = bounds.minY - (0.5 * gridSize);
    const int gridCountX = static_cast<int>(((bounds.maxX - gridMinX) / gridSize)) + 1;
    const int gridCountY = static_cast<int>(((bounds.maxY - gridMinY) / gridSize)) + 1;

    QStringList sketchRows;
    for (const PocketTopoPolyline &polyline : polylines) {
        if (polyline.points.isEmpty()) {
            continue;
        }

        QStringList tokens;
        tokens.append(polyline.colorToken);
        for (const QPointF &point : polyline.points) {
            tokens.append(formatPocketTopoCoordinate(point.x()));
            tokens.append(formatPocketTopoCoordinate(point.y()));
        }
        sketchRows.append(QStringLiteral("  {%1}").arg(tokens.join(QLatin1Char(' '))));
    }

    QString output;
    output += QStringLiteral("set XVIgrids {%1 m}\n").arg(formatPocketTopoXviNumber(gridSpacingMeters));
    output += QStringLiteral("set XVIstations {\n%1\n}\n").arg(stationRows.join(QLatin1Char('\n')));
    output += QStringLiteral("set XVIshots {\n%1\n}\n").arg(shotRows.join(QLatin1Char('\n')));
    output += QStringLiteral("set XVIsketchlines {\n%1\n}\n").arg(sketchRows.join(QLatin1Char('\n')));
    output += QStringLiteral("set XVIgrid {%1 %2 %3 0.0 0.0 %4 %5 %6}\n")
                  .arg(formatPocketTopoXviNumber(gridMinX),
                       formatPocketTopoXviNumber(gridMinY),
                       formatPocketTopoXviNumber(gridSize),
                       formatPocketTopoXviNumber(gridSize),
                       QString::number(qMax(1, gridCountX)),
                       QString::number(qMax(1, gridCountY)));
    return output;
}
}
