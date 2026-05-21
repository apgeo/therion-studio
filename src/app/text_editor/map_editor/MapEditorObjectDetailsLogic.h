#pragma once

#include <QPointF>
#include <QString>

#include <optional>

namespace TherionStudio
{
struct TherionParsedLine;

bool sourcePointsDifferForCommands(const QPointF &a, const QPointF &b);
bool isConfigurableMapObjectKind(const QString &kind);
QString objectKindForDirective(const QString &directiveToken);
qreal normalizeOrientationDegreesForMapDetails(qreal value);
bool isOrientationSupportedForParsedLine(const TherionParsedLine &parsedLine);
bool isLinePointLeftSizeSupportedForParsedLine(const TherionParsedLine &parsedLine);
std::optional<qreal> pointOrientationFromParsedLine(const TherionParsedLine &parsedLine);
std::optional<qreal> linePointOrientationForSourceVertex(const QString &documentText,
                                                         int lineNumber,
                                                         int sourceVertexIndex);
std::optional<qreal> linePointLeftSizeForSourceVertex(const QString &documentText,
                                                      int lineNumber,
                                                      int sourceVertexIndex);
}
