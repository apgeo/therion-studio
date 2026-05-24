#include "MapEditorInspectorData.h"

#include "MapEditorSceneSupport.h"
#include "../../../core/CommandCatalogService.h"
#include "../../../core/ProjectStructureIndex.h"
#include "../../../core/TherionDocumentParser.h"

#include <QComboBox>
#include <QApplication>
#include <QFile>
#include <QHash>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLineEdit>
#include <QPainter>
#include <QPalette>
#include <QSvgRenderer>

namespace TherionStudio
{
namespace
{
QPixmap renderInspectorLucidePixmap(const QString &iconName, const QColor &color, int extent)
{
    QFile file(QStringLiteral(":/resources/icons/lucide/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return QPixmap();
    }

    QString svg = QString::fromUtf8(file.readAll());
    svg.replace(QStringLiteral("currentColor"), color.name(QColor::HexRgb));
    QSvgRenderer renderer(svg.toUtf8());
    if (!renderer.isValid()) {
        return QPixmap();
    }

    const qreal devicePixelRatio = qApp != nullptr ? qApp->devicePixelRatio() : 1.0;
    QPixmap pixmap(QSize(extent, extent) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, extent, extent));
    return pixmap;
}
}

bool inspectorTokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    token.toDouble(&ok);
    return ok;
}

QString inspectorOptionValue(const QStringList &tokens, const QString &option)
{
    for (int index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens.at(index) == option) {
            return tokens.at(index + 1);
        }
    }

    return QString();
}

QString inspectorBracketedOptionValue(const QStringList &tokens, const QString &option)
{
    for (int index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens.at(index) != option) {
            continue;
        }

        const QString firstValue = tokens.at(index + 1);
        if (!firstValue.contains(QLatin1Char('[')) || firstValue.contains(QLatin1Char(']'))) {
            return firstValue;
        }

        QStringList valueTokens;
        valueTokens.append(firstValue);
        for (int valueIndex = index + 2; valueIndex < tokens.size(); ++valueIndex) {
            const QString token = tokens.at(valueIndex);
            if (token.startsWith(QLatin1Char('-')) && !inspectorTokenLooksNumeric(token)) {
                break;
            }
            valueTokens.append(token);
            if (token.contains(QLatin1Char(']'))) {
                break;
            }
        }
        return valueTokens.join(QLatin1Char(' '));
    }

    return QString();
}

QString inspectorPointTypeToken(const TherionParsedLine &parsedLine)
{
    bool skipOptionValue = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (skipOptionValue) {
            skipOptionValue = false;
            continue;
        }
        if (inspectorTokenLooksNumeric(token)) {
            continue;
        }
        if (token.startsWith(QLatin1Char('-'))) {
            skipOptionValue = index + 1 < parsedLine.tokens.size();
            continue;
        }

        return token;
    }

    return QString();
}

QString inspectorStationNameToken(const TherionParsedLine &parsedLine)
{
    bool skipOptionValue = false;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (skipOptionValue) {
            skipOptionValue = false;
            continue;
        }
        if (token.startsWith(QLatin1Char('-'))) {
            skipOptionValue = index + 1 < parsedLine.tokens.size();
            continue;
        }
        if (token.compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0 || inspectorTokenLooksNumeric(token)) {
            continue;
        }

        return token;
    }

    return QString();
}

QString inspectorObjectTextWithOptionalIdentifier(const QString &type, const QString &subtype, const QString &identifier)
{
    const QString normalizedType = type.trimmed();
    const QString normalizedSubtype = subtype.trimmed();
    const QString normalizedIdentifier = identifier.trimmed();

    QString text = normalizedSubtype.isEmpty()
        ? normalizedType
        : QStringLiteral("%1 %2").arg(normalizedType, normalizedSubtype);
    if (!normalizedIdentifier.isEmpty()) {
        text = QStringLiteral("%1: %2").arg(text, normalizedIdentifier);
    }

    return text;
}

QString inspectorTypePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 0, 0).trimmed();
}

QString inspectorInlineSubtypePart(const QString &typeToken)
{
    return typeToken.section(QLatin1Char(':'), 1).trimmed();
}

QString inspectorObjectSubtype(const TherionParsedLine &parsedLine, const QString &typeToken)
{
    const QString optionSubtype = inspectorOptionValue(parsedLine.tokens, QStringLiteral("-subtype"));
    if (!optionSubtype.isEmpty()) {
        return optionSubtype;
    }

    return inspectorInlineSubtypePart(typeToken);
}

QString inspectorMapObjectIconName(const ProjectStructureEntry &entry)
{
    if (entry.category == QStringLiteral("Scraps")) {
        return QStringLiteral("puzzle");
    }
    if (entry.category == QStringLiteral("Stations") || entry.category == QStringLiteral("Points")) {
        return QStringLiteral("locate-fixed");
    }
    if (entry.category == QStringLiteral("Lines")) {
        return QStringLiteral("spline");
    }
    if (entry.category == QStringLiteral("Areas")) {
        return QStringLiteral("pentagon");
    }

    return QString();
}

QString inspectorMapObjectItemText(const ProjectStructureEntry &entry, const TherionParsedLine *parsedLine)
{
    if (entry.category == QStringLiteral("Scraps")) {
        return entry.name;
    }

    if (parsedLine == nullptr) {
        return entry.name;
    }

    if (entry.category == QStringLiteral("Stations") || entry.category == QStringLiteral("Points")) {
        const QString pointTypeToken = inspectorPointTypeToken(*parsedLine);
        const QString pointType = entry.category == QStringLiteral("Stations")
            ? QStringLiteral("station")
            : inspectorTypePart(pointTypeToken);
        const QString pointSubtype = inspectorObjectSubtype(*parsedLine, pointTypeToken);
        const QString stationName = inspectorOptionValue(parsedLine->tokens, QStringLiteral("-name"));
        QString identifier = stationName.isEmpty() ? inspectorOptionValue(parsedLine->tokens, QStringLiteral("-id")) : stationName;
        if (entry.category == QStringLiteral("Stations") && identifier.isEmpty()) {
            identifier = inspectorStationNameToken(*parsedLine);
        }
        return inspectorObjectTextWithOptionalIdentifier(pointType.isEmpty() ? entry.name : pointType,
                                                         pointSubtype,
                                                         identifier);
    }

    if (entry.category == QStringLiteral("Lines")) {
        const QString lineTypeToken = parsedLine->tokens.value(1, entry.name);
        return inspectorObjectTextWithOptionalIdentifier(inspectorTypePart(lineTypeToken),
                                                         inspectorObjectSubtype(*parsedLine, lineTypeToken),
                                                         inspectorOptionValue(parsedLine->tokens, QStringLiteral("-id")));
    }

    if (entry.category == QStringLiteral("Areas")) {
        const QString areaTypeToken = parsedLine->tokens.value(1, entry.name);
        return inspectorObjectTextWithOptionalIdentifier(inspectorTypePart(areaTypeToken),
                                                         inspectorObjectSubtype(*parsedLine, areaTypeToken),
                                                         inspectorOptionValue(parsedLine->tokens, QStringLiteral("-id")));
    }

    return entry.name;
}

QIcon inspectorActionIcon(const QString &iconName)
{
    const QPalette palette = QApplication::palette();
    QIcon icon;
    icon.addPixmap(renderInspectorLucidePixmap(iconName, palette.color(QPalette::Text), 16), QIcon::Normal);
    icon.addPixmap(renderInspectorLucidePixmap(iconName, palette.color(QPalette::Disabled, QPalette::Text), 16), QIcon::Disabled);
    return icon;
}

qreal mapSourceUnitsPerMeterFromParsedLines(const QVector<TherionParsedLine> &parsedLines)
{
    QVector<qreal> candidates;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("scrap")) {
            continue;
        }

        const std::optional<qreal> sourceUnitsPerMeter = sourceUnitsPerMeterFromScrapScale(parsedLine.tokens);
        if (sourceUnitsPerMeter.has_value()) {
            candidates.append(sourceUnitsPerMeter.value());
        }
    }

    if (candidates.isEmpty()) {
        return 1.0;
    }

    std::sort(candidates.begin(), candidates.end());
    return candidates.at(candidates.size() / 2);
}

QString formatTherionScaleNumber(qreal value)
{
    if (std::fabs(value) < 1e-9) {
        return QStringLiteral("0.0");
    }

    const qreal nearestInteger = std::round(value);
    if (std::fabs(value - nearestInteger) < 1e-9) {
        return QString::number(static_cast<qlonglong>(nearestInteger));
    }

    return QString::number(value, 'g', 12);
}

QString xtherionDefaultScrapScaleOption(const QRectF &sourceBounds)
{
    QRectF bounds = sourceBounds.normalized();
    if (!bounds.isValid() || bounds.width() < 1e-6) {
        bounds = QRectF(0.0, 0.0, 1600.0, 1200.0);
    }

    const qreal left = bounds.left();
    const qreal right = bounds.right();
    const qreal top = bounds.top();
    const qreal realLengthMeters = 0.0254 * (right - left);

    return QStringLiteral("-scale [%1 %2 %3 %4 0.0 0.0 %5 0.0 m]")
        .arg(formatTherionScaleNumber(left),
             formatTherionScaleNumber(top),
             formatTherionScaleNumber(right),
             formatTherionScaleNumber(top),
             formatTherionScaleNumber(realLengthMeters));
}

QString cleanedScaleToken(QString token)
{
    token = token.trimmed();
    while (token.startsWith(QLatin1Char('['))) {
        token.remove(0, 1);
    }
    while (token.endsWith(QLatin1Char(']'))) {
        token.chop(1);
    }
    return token.trimmed();
}

bool parseInspectorScaleNumber(const QString &token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    const QString cleaned = cleanedScaleToken(token);
    if (cleaned.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qreal parsed = cleaned.toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

QString normalizedInspectorScaleUnit(QString token)
{
    token = cleanedScaleToken(token).toLower();
    if (token == QStringLiteral("meter") || token == QStringLiteral("meters") || token == QStringLiteral("metre") || token == QStringLiteral("metres")) {
        return QStringLiteral("m");
    }
    if (token == QStringLiteral("centimeter") || token == QStringLiteral("centimeters") || token == QStringLiteral("centimetre") || token == QStringLiteral("centimetres")) {
        return QStringLiteral("cm");
    }
    if (token == QStringLiteral("millimeter") || token == QStringLiteral("millimeters") || token == QStringLiteral("millimetre") || token == QStringLiteral("millimetres")) {
        return QStringLiteral("mm");
    }
    if (token == QStringLiteral("foot") || token == QStringLiteral("feet")) {
        return QStringLiteral("ft");
    }
    if (token == QStringLiteral("inch") || token == QStringLiteral("inches")) {
        return QStringLiteral("in");
    }
    if (token == QStringLiteral("m") || token == QStringLiteral("cm") || token == QStringLiteral("mm") || token == QStringLiteral("ft") || token == QStringLiteral("in")) {
        return token;
    }
    return QStringLiteral("m");
}

std::optional<InspectorScrapScale> inspectorScrapScaleFromTokens(const QStringList &tokens)
{
    const int scaleOptionIndex = tokens.indexOf(QStringLiteral("-scale"));
    if (scaleOptionIndex < 0 || scaleOptionIndex + 1 >= tokens.size()) {
        return std::nullopt;
    }

    QVector<qreal> numbers;
    numbers.reserve(8);
    QString unitToken = QStringLiteral("m");
    const bool bracketed = tokens.at(scaleOptionIndex + 1).contains(QLatin1Char('['));
    for (int index = scaleOptionIndex + 1; index < tokens.size(); ++index) {
        const QString token = tokens.at(index);
        if (!bracketed && token.startsWith(QLatin1Char('-')) && !inspectorTokenLooksNumeric(token)) {
            break;
        }

        qreal parsedNumber = 0.0;
        if (parseInspectorScaleNumber(token, &parsedNumber)) {
            numbers.append(parsedNumber);
        } else {
            unitToken = normalizedInspectorScaleUnit(token);
        }

        if (!bracketed || token.contains(QLatin1Char(']'))) {
            break;
        }
    }

    if (numbers.size() < 8) {
        return std::nullopt;
    }

    InspectorScrapScale scale;
    scale.sourcePoint1 = QPointF(numbers.at(0), numbers.at(1));
    scale.sourcePoint2 = QPointF(numbers.at(2), numbers.at(3));
    scale.realPoint1 = QPointF(numbers.at(4), numbers.at(5));
    scale.realPoint2 = QPointF(numbers.at(6), numbers.at(7));
    scale.unitToken = unitToken;
    return scale;
}

QString scrapScaleExpression(const InspectorScrapScale &scale)
{
    return QStringLiteral("[%1 %2 %3 %4 %5 %6 %7 %8 %9]")
        .arg(formatTherionScaleNumber(scale.sourcePoint1.x()),
             formatTherionScaleNumber(scale.sourcePoint1.y()),
             formatTherionScaleNumber(scale.sourcePoint2.x()),
             formatTherionScaleNumber(scale.sourcePoint2.y()),
             formatTherionScaleNumber(scale.realPoint1.x()),
             formatTherionScaleNumber(scale.realPoint1.y()),
             formatTherionScaleNumber(scale.realPoint2.x()),
             formatTherionScaleNumber(scale.realPoint2.y()),
             scale.unitToken.isEmpty() ? QStringLiteral("m") : scale.unitToken);
}

InspectorScrapScale defaultInspectorScrapScale(const QRectF &sourceBounds)
{
    QRectF bounds = sourceBounds.normalized();
    if (!bounds.isValid() || bounds.width() < 1e-6) {
        bounds = QRectF(0.0, 0.0, 1600.0, 1200.0);
    }

    InspectorScrapScale scale;
    scale.sourcePoint1 = QPointF(bounds.left(), bounds.top());
    scale.sourcePoint2 = QPointF(bounds.right(), bounds.top());
    scale.realPoint1 = QPointF(0.0, 0.0);
    scale.realPoint2 = QPointF((bounds.right() - bounds.left()) * 0.0254, 0.0);
    scale.unitToken = QStringLiteral("m");
    return scale;
}

struct InspectorSymbolCatalog
{
    QHash<QString, QStringList> typeValuesByCommand;
    QHash<QString, QHash<QString, QStringList>> subtypeValuesByCommandAndType;
    QStringList projectionValues;
};

void appendInspectorCatalogToken(QStringList &tokens, const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty() || normalized == QStringLiteral("*") || tokens.contains(normalized)) {
        return;
    }

    tokens.append(normalized);
}

QStringList defaultInspectorProjectionValues()
{
    return {
        QStringLiteral("plan"),
        QStringLiteral("extended"),
        QStringLiteral("elevation"),
        QStringLiteral("none"),
    };
}

InspectorSymbolCatalog loadInspectorSymbolCatalog()
{
    InspectorSymbolCatalog catalog;

    const CommandCatalogStore catalogStore;
    const QJsonObject catalogObject = catalogStore.catalogObject();
    if (catalogObject.isEmpty()) {
        catalog.projectionValues = defaultInspectorProjectionValues();
        return catalog;
    }

    const QJsonArray commands = catalogObject.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commands) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (commandName == QStringLiteral("scrap")) {
            const QJsonArray options = commandObject.value(QStringLiteral("options")).toArray();
            for (const QJsonValue &optionValue : options) {
                const QJsonObject optionObject = optionValue.toObject();
                if (optionObject.value(QStringLiteral("option_key")).toString().trimmed().toLower()
                    != QStringLiteral("-projection")) {
                    continue;
                }
                const QJsonArray allowedValues = optionObject.value(QStringLiteral("allowed_values")).toArray();
                for (const QJsonValue &allowedValue : allowedValues) {
                    appendInspectorCatalogToken(catalog.projectionValues, allowedValue.toString());
                }
            }
        }

        if (commandName != QStringLiteral("point")
            && commandName != QStringLiteral("line")
            && commandName != QStringLiteral("area")) {
            continue;
        }

        const QJsonArray typeValues = commandObject.value(QStringLiteral("type_values")).toArray();
        for (const QJsonValue &typeValue : typeValues) {
            appendInspectorCatalogToken(catalog.typeValuesByCommand[commandName], typeValue.toString());
        }

        const QJsonObject subtypeByType = commandObject.value(QStringLiteral("subtype_by_type")).toObject();
        for (auto subtypeIterator = subtypeByType.begin(); subtypeIterator != subtypeByType.end(); ++subtypeIterator) {
            const QString typeKey = subtypeIterator.key().trimmed().toLower();
            if (typeKey.isEmpty()) {
                continue;
            }

            const QJsonArray subtypeValues = subtypeIterator.value().toArray();
            for (const QJsonValue &subtypeValue : subtypeValues) {
                appendInspectorCatalogToken(catalog.subtypeValuesByCommandAndType[commandName][typeKey],
                                            subtypeValue.toString());
            }
        }
    }

    if (catalog.projectionValues.isEmpty()) {
        catalog.projectionValues = defaultInspectorProjectionValues();
    }

    return catalog;
}

const InspectorSymbolCatalog &inspectorSymbolCatalog()
{
    static const InspectorSymbolCatalog catalog = loadInspectorSymbolCatalog();
    return catalog;
}

QStringList inspectorTypeValuesForCommand(const QString &commandKind)
{
    return inspectorSymbolCatalog().typeValuesByCommand.value(commandKind.trimmed().toLower());
}

QStringList inspectorSubtypeValuesForCommandType(const QString &commandKind, const QString &type)
{
    return inspectorSymbolCatalog()
        .subtypeValuesByCommandAndType
        .value(commandKind.trimmed().toLower())
        .value(type.trimmed().toLower());
}

QStringList inspectorProjectionValues()
{
    return inspectorSymbolCatalog().projectionValues;
}

void setEditableComboValues(QComboBox *combo, const QStringList &values, const QString &currentText)
{
    if (combo == nullptr) {
        return;
    }

    const bool comboSignalsBlocked = combo->blockSignals(true);
    QLineEdit *lineEdit = combo->lineEdit();
    const bool lineEditSignalsBlocked = lineEdit != nullptr ? lineEdit->blockSignals(true) : false;
    combo->clear();
    combo->addItems(values);
    combo->setCurrentText(currentText);
    if (lineEdit != nullptr) {
        lineEdit->blockSignals(lineEditSignalsBlocked);
    }
    combo->blockSignals(comboSignalsBlocked);
}

std::optional<InspectorObjectQuickFields> inspectorObjectQuickFieldsFromParsedLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive == QStringLiteral("scrap")) {
        if (parsedLine.tokens.size() < 2) {
            return std::nullopt;
        }

        InspectorObjectQuickFields fields;
        fields.commandKind = parsedLine.directive;
        fields.identifier = parsedLine.tokens.at(1);
        fields.projection = inspectorBracketedOptionValue(parsedLine.tokens, QStringLiteral("-projection"));
        fields.typeEditable = false;
        return fields;
    }

    if (parsedLine.directive == QStringLiteral("point")) {
        const QString pointTypeToken = inspectorPointTypeToken(parsedLine);
        InspectorObjectQuickFields fields;
        fields.commandKind = parsedLine.directive;
        fields.type = inspectorTypePart(pointTypeToken);
        fields.subtype = inspectorObjectSubtype(parsedLine, pointTypeToken);
        const bool station = fields.type.compare(QStringLiteral("station"), Qt::CaseInsensitive) == 0;
        fields.identifier = inspectorOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        fields.name = inspectorOptionValue(parsedLine.tokens, QStringLiteral("-name"));
        if (fields.name.isEmpty() && station) {
            fields.name = inspectorStationNameToken(parsedLine);
        }
        fields.nameVisible = station || !fields.name.isEmpty();
        return fields;
    }

    if (parsedLine.directive == QStringLiteral("line") || parsedLine.directive == QStringLiteral("area")) {
        if (parsedLine.tokens.size() < 2) {
            return std::nullopt;
        }

        const QString typeToken = parsedLine.tokens.at(1);
        InspectorObjectQuickFields fields;
        fields.commandKind = parsedLine.directive;
        fields.type = inspectorTypePart(typeToken);
        fields.subtype = inspectorObjectSubtype(parsedLine, typeToken);
        fields.identifier = inspectorOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        return fields;
    }

    return std::nullopt;
}

}
