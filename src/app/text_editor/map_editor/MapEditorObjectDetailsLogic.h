#pragma once

#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace TherionStudio
{
struct TherionParsedLine;

struct MapEditorOrientationApplicability
{
    QSet<QString> allowedTypes;
    QSet<QString> excludedTypes;
};

using MapEditorOrientationApplicabilityByCommand = QHash<QString, MapEditorOrientationApplicability>;

bool sourcePointsDifferForCommands(const QPointF &a, const QPointF &b);
bool isConfigurableMapObjectKind(const QString &kind);
QString objectKindForDirective(const QString &directiveToken);
qreal normalizeOrientationDegreesForMapDetails(qreal value);
MapEditorOrientationApplicabilityByCommand mapEditorOrientationApplicabilityFromCommandCatalog(const QJsonObject &catalogObject);
bool isOrientationSupportedForParsedLine(const TherionParsedLine &parsedLine,
                                         const MapEditorOrientationApplicabilityByCommand &applicabilityByCommand);
bool isLinePointLeftSizeSupportedForParsedLine(const TherionParsedLine &parsedLine);
std::optional<qreal> pointOrientationFromParsedLine(const TherionParsedLine &parsedLine);
std::optional<qreal> linePointOrientationForSourceVertex(const QString &documentText,
                                                         int lineNumber,
                                                         int sourceVertexIndex);
std::optional<qreal> linePointLeftSizeForSourceVertex(const QString &documentText,
                                                      int lineNumber,
                                                      int sourceVertexIndex);
QString linePointSegmentSubtypeFromStandaloneRows(const QStringList &rows);
bool linePointAltitudeAutoFromStandaloneRows(const QStringList &rows);
QStringList linePointRowsWithoutStructuredStandaloneOptions(const QStringList &rows,
                                                            bool manageSegmentSubtype = true,
                                                            bool manageAltitudeAuto = true);
QStringList linePointRowsWithStructuredStandaloneOptions(const QStringList &rows,
                                                         const QString &segmentSubtype,
                                                         bool altitudeAuto,
                                                         bool manageSegmentSubtype = true,
                                                         bool manageAltitudeAuto = true);
}
