#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

class QComboBox;
class QIcon;

namespace TherionStudio
{
struct ProjectStructureEntry;
struct TherionParsedLine;

struct InspectorObjectQuickFields
{
    QString commandKind;
    QString type;
    QString subtype;
    QString projection;
    QString identifier;
    QString name;
    bool nameVisible = false;
    bool typeEditable = true;
};

struct InspectorScrapScale
{
    QPointF sourcePoint1;
    QPointF sourcePoint2;
    QPointF realPoint1;
    QPointF realPoint2;
    QString unitToken = QStringLiteral("m");
};

qreal mapSourceUnitsPerMeterFromParsedLines(const QVector<TherionParsedLine> &parsedLines);
QString xtherionDefaultScrapScaleOption(const QRectF &sourceBounds);
std::optional<InspectorScrapScale> inspectorScrapScaleFromTokens(const QStringList &tokens);
QString scrapScaleExpression(const InspectorScrapScale &scale);
InspectorScrapScale defaultInspectorScrapScale(const QRectF &sourceBounds);
QString inspectorMapObjectIconName(const ProjectStructureEntry &entry);
QString inspectorMapObjectItemText(const ProjectStructureEntry &entry, const TherionParsedLine *parsedLine);
QIcon inspectorActionIcon(const QString &iconName);
QStringList inspectorTypeValuesForCommand(const QString &commandKind);
QStringList inspectorSubtypeValuesForCommandType(const QString &commandKind, const QString &type);
QStringList inspectorProjectionValues();
void setEditableComboValues(QComboBox *combo, const QStringList &values, const QString &currentText);
std::optional<InspectorObjectQuickFields> inspectorObjectQuickFieldsFromParsedLine(const TherionParsedLine &parsedLine);
}
