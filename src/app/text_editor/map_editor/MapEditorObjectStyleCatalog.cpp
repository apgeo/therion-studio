#include "MapEditorObjectStyleCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cmath>
#include <optional>

namespace TherionStudio
{
namespace
{
std::optional<qreal> optionalPositiveNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (number <= 0.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<qreal> optionalNonNegativeNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (number < 0.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<qreal> optionalZeroToOneNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (number < 0.0 || number > 1.0) {
        return std::nullopt;
    }
    return number;
}

std::optional<QColor> optionalColor(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) {
        return std::nullopt;
    }

    const QColor color(value.toString().trimmed());
    if (!color.isValid()) {
        return std::nullopt;
    }
    return color;
}

Qt::PenStyle penStyleFromString(const QString &value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("dash") || normalized == QStringLiteral("dashed")) {
        return Qt::DashLine;
    }
    if (normalized == QStringLiteral("dot") || normalized == QStringLiteral("dotted")) {
        return Qt::DotLine;
    }
    if (normalized == QStringLiteral("dash-dot")) {
        return Qt::DashDotLine;
    }
    if (normalized == QStringLiteral("dash-dot-dot")) {
        return Qt::DashDotDotLine;
    }
    return Qt::SolidLine;
}

std::optional<int> optionalInteger(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (!std::isfinite(number)) {
        return std::nullopt;
    }
    return static_cast<int>(std::llround(number));
}

std::optional<QVector<qreal>> optionalDashPattern(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isArray()) {
        return std::nullopt;
    }

    const QJsonArray array = value.toArray();
    if (array.isEmpty()) {
        return QVector<qreal>{};
    }

    QVector<qreal> pattern;
    pattern.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isDouble()) {
            return std::nullopt;
        }
        const qreal number = entry.toDouble();
        if (number <= 0.0) {
            return std::nullopt;
        }
        pattern.append(number);
    }

    return pattern;
}

QString normalizedToken(const QString &value)
{
    return value.trimmed().toLower();
}

MapEditorFillPatternKind fillPatternKindFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("hatch")) {
        return MapEditorFillPatternKind::Hatch;
    }
    if (normalized == QStringLiteral("cross_hatch")) {
        return MapEditorFillPatternKind::CrossHatch;
    }
    if (normalized == QStringLiteral("dots")) {
        return MapEditorFillPatternKind::Dots;
    }
    return MapEditorFillPatternKind::None;
}

std::optional<MapEditorAreaFillPatternStyle> readAreaFillPattern(const QJsonObject &object)
{
    if (object.isEmpty()) {
        return std::nullopt;
    }

    MapEditorAreaFillPatternStyle pattern;
    pattern.kind = fillPatternKindFromString(object.value(QStringLiteral("kind")).toString());
    if (pattern.kind == MapEditorFillPatternKind::None) {
        return std::nullopt;
    }

    if (const std::optional<qreal> spacing = optionalPositiveNumber(object, "spacing")) {
        pattern.spacing = spacing.value();
    }
    if (object.value(QStringLiteral("stroke_angle")).isDouble()) {
        pattern.angle = object.value(QStringLiteral("stroke_angle")).toDouble(pattern.angle);
    } else if (object.value(QStringLiteral("angle")).isDouble()) {
        pattern.angle = object.value(QStringLiteral("angle")).toDouble(pattern.angle);
    }
    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(object, "stroke_width")) {
        pattern.strokeWidth = strokeWidth.value();
    }
    if (object.value(QStringLiteral("stroke_style")).isString()) {
        pattern.strokeStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
    }
    pattern.strokeColor = optionalColor(object, "stroke_color");
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
        pattern.dashPattern = dashPattern.value();
    }
    if (const std::optional<qreal> radius = optionalPositiveNumber(object, "radius")) {
        pattern.radius = radius.value();
    }
    pattern.dotColor = optionalColor(object, "dot_color");
    if (const std::optional<qreal> angleJitter = optionalNonNegativeNumber(object, "angle_jitter")) {
        pattern.angleJitter = angleJitter.value();
    }
    if (const std::optional<qreal> offsetJitter = optionalNonNegativeNumber(object, "offset_jitter")) {
        pattern.offsetJitter = offsetJitter.value();
    }
    if (const std::optional<int> seed = optionalInteger(object, "seed")) {
        pattern.seed = seed;
    }

    return pattern;
}

MapEditorStyleSelector readSelector(const QJsonObject &object)
{
    MapEditorStyleSelector selector;
    selector.rawType = normalizedToken(object.value(QStringLiteral("raw_type")).toString());
    selector.subtype = normalizedToken(object.value(QStringLiteral("raw_subtype")).toString());
    return selector;
}

bool selectorMatches(const MapEditorStyleSelector &selector,
                    const QString &rawType,
                    const QString &subtype)
{
    if (selector.rawType.isEmpty()) {
        return false;
    }
    if (selector.rawType != rawType) {
        return false;
    }
    if (selector.subtype.isEmpty()) {
        return true;
    }
    return selector.subtype == subtype;
}

MapEditorObjectStyleCatalog loadMapEditorObjectStyleCatalogFromResource()
{
    MapEditorObjectStyleCatalog catalog;

    QFile file(QStringLiteral(":/resources/map_object_styles.json"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return catalog;
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        return catalog;
    }

    const QJsonObject root = json.object();
    const QJsonObject defaults = root.value(QStringLiteral("defaults")).toObject();

    const QJsonObject point = defaults.value(QStringLiteral("point")).toObject();
    if (!point.isEmpty()) {
        if (const std::optional<qreal> radius = optionalPositiveNumber(point, "radius")) {
            catalog.point.radius = radius.value();
        }
        if (const std::optional<qreal> outlineWidth = optionalPositiveNumber(point, "stroke_width")) {
            catalog.point.outlineWidth = outlineWidth.value();
        }
        catalog.point.fillColor = optionalColor(point, "fill_color");
        catalog.point.strokeColor = optionalColor(point, "stroke_color");
    }

    const QJsonObject line = defaults.value(QStringLiteral("line")).toObject();
    if (!line.isEmpty()) {
        if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(line, "stroke_width")) {
            catalog.line.strokeWidth = strokeWidth.value();
        }
        if (line.value(QStringLiteral("stroke_style")).isString()) {
            catalog.line.penStyle = penStyleFromString(line.value(QStringLiteral("stroke_style")).toString());
        }
        catalog.line.strokeColor = optionalColor(line, "stroke_color");
        if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(line, "dash_pattern")) {
            catalog.line.dashPattern = dashPattern.value();
        }
    }

    const QJsonObject area = defaults.value(QStringLiteral("area")).toObject();
    if (!area.isEmpty()) {
        if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(area, "stroke_width")) {
            catalog.area.strokeWidth = strokeWidth.value();
        }
        if (const std::optional<qreal> fillOpacity = optionalZeroToOneNumber(area, "fill_opacity")) {
            catalog.area.fillOpacity = fillOpacity.value();
        }
        if (area.value(QStringLiteral("stroke_style")).isString()) {
            catalog.area.penStyle = penStyleFromString(area.value(QStringLiteral("stroke_style")).toString());
        }
        catalog.area.strokeColor = optionalColor(area, "stroke_color");
        catalog.area.fillColor = optionalColor(area, "fill_color");
        if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(area, "dash_pattern")) {
            catalog.area.dashPattern = dashPattern.value();
        }
        catalog.area.fillPattern = readAreaFillPattern(area.value(QStringLiteral("fill_pattern")).toObject());
    }

    const QJsonArray pointStyles = root.value(QStringLiteral("point_styles")).toArray();
    catalog.pointStyles.reserve(pointStyles.size());
    for (const QJsonValue &entry : pointStyles) {
        if (!entry.isObject()) {
            continue;
        }
        const QJsonObject object = entry.toObject();
        MapEditorPointStyleRule rule;
        rule.selector = readSelector(object);
        if (rule.selector.rawType.isEmpty()) {
            continue;
        }
        rule.radius = optionalPositiveNumber(object, "radius");
        rule.outlineWidth = optionalPositiveNumber(object, "stroke_width");
        rule.fillColor = optionalColor(object, "fill_color");
        rule.strokeColor = optionalColor(object, "stroke_color");
        catalog.pointStyles.append(rule);
    }

    const QJsonArray lineStyles = root.value(QStringLiteral("line_styles")).toArray();
    catalog.lineStyles.reserve(lineStyles.size());
    for (const QJsonValue &entry : lineStyles) {
        if (!entry.isObject()) {
            continue;
        }
        const QJsonObject object = entry.toObject();
        MapEditorLineStyleRule rule;
        rule.selector = readSelector(object);
        if (rule.selector.rawType.isEmpty()) {
            continue;
        }
        rule.strokeWidth = optionalPositiveNumber(object, "stroke_width");
        if (object.value(QStringLiteral("stroke_style")).isString()) {
            rule.penStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
        }
        rule.strokeColor = optionalColor(object, "stroke_color");
        if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
            rule.dashPattern = dashPattern;
        }
        catalog.lineStyles.append(rule);
    }

    const QJsonArray areaStyles = root.value(QStringLiteral("area_styles")).toArray();
    catalog.areaStyles.reserve(areaStyles.size());
    for (const QJsonValue &entry : areaStyles) {
        if (!entry.isObject()) {
            continue;
        }
        const QJsonObject object = entry.toObject();
        MapEditorAreaStyleRule rule;
        rule.selector = readSelector(object);
        if (rule.selector.rawType.isEmpty()) {
            continue;
        }

        rule.strokeWidth = optionalPositiveNumber(object, "stroke_width");
        rule.fillOpacity = optionalZeroToOneNumber(object, "fill_opacity");
        if (object.value(QStringLiteral("stroke_style")).isString()) {
            rule.penStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
        }

        rule.strokeColor = optionalColor(object, "stroke_color");
        rule.fillColor = optionalColor(object, "fill_color");
        if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
            rule.dashPattern = dashPattern;
        }

        rule.fillPattern = readAreaFillPattern(object.value(QStringLiteral("fill_pattern")).toObject());

        catalog.areaStyles.append(rule);
    }

    catalog.loadedFromResource = true;
    return catalog;
}
} // namespace

MapEditorObjectStyleCatalog mapEditorObjectStyleCatalog()
{
    static const MapEditorObjectStyleCatalog catalog = loadMapEditorObjectStyleCatalogFromResource();
    return catalog;
}

MapEditorResolvedPointStyle resolveMapEditorPointStyle(const MapEditorObjectStyleCatalog &catalog,
                                                       const QString &rawType,
                                                       const QString &subtype)
{
    MapEditorResolvedPointStyle resolved;
    resolved.radius = catalog.point.radius;
    resolved.outlineWidth = catalog.point.outlineWidth;
    resolved.fillColor = catalog.point.fillColor;
    resolved.strokeColor = catalog.point.strokeColor;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorPointStyleRule &rule : catalog.pointStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.radius.has_value()) {
            resolved.radius = rule.radius.value();
        }
        if (rule.outlineWidth.has_value()) {
            resolved.outlineWidth = rule.outlineWidth.value();
        }
        if (rule.fillColor.has_value()) {
            resolved.fillColor = rule.fillColor;
        }
        if (rule.strokeColor.has_value()) {
            resolved.strokeColor = rule.strokeColor;
        }
    }

    return resolved;
}

MapEditorResolvedLineStyle resolveMapEditorLineStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype)
{
    MapEditorResolvedLineStyle resolved;
    resolved.strokeWidth = catalog.line.strokeWidth;
    resolved.penStyle = catalog.line.penStyle;
    resolved.strokeColor = catalog.line.strokeColor;
    resolved.dashPattern = catalog.line.dashPattern;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorLineStyleRule &rule : catalog.lineStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.strokeWidth.has_value()) {
            resolved.strokeWidth = rule.strokeWidth.value();
        }
        if (rule.penStyle.has_value()) {
            resolved.penStyle = rule.penStyle.value();
        }
        if (rule.strokeColor.has_value()) {
            resolved.strokeColor = rule.strokeColor;
        }
        if (rule.dashPattern.has_value()) {
            resolved.dashPattern = rule.dashPattern.value();
        }
    }

    return resolved;
}

MapEditorResolvedAreaStyle resolveMapEditorAreaStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype)
{
    MapEditorResolvedAreaStyle resolved;
    resolved.strokeWidth = catalog.area.strokeWidth;
    resolved.fillOpacity = catalog.area.fillOpacity;
    resolved.penStyle = catalog.area.penStyle;
    resolved.strokeColor = catalog.area.strokeColor;
    resolved.fillColor = catalog.area.fillColor;
    resolved.dashPattern = catalog.area.dashPattern;
    resolved.fillPattern = catalog.area.fillPattern;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorAreaStyleRule &rule : catalog.areaStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.strokeWidth.has_value()) {
            resolved.strokeWidth = rule.strokeWidth.value();
        }
        if (rule.fillOpacity.has_value()) {
            resolved.fillOpacity = rule.fillOpacity.value();
        }
        if (rule.penStyle.has_value()) {
            resolved.penStyle = rule.penStyle.value();
        }
        if (rule.strokeColor.has_value()) {
            resolved.strokeColor = rule.strokeColor;
        }
        if (rule.fillColor.has_value()) {
            resolved.fillColor = rule.fillColor;
        }
        if (rule.dashPattern.has_value()) {
            resolved.dashPattern = rule.dashPattern.value();
        }
        if (rule.fillPattern.has_value()) {
            resolved.fillPattern = rule.fillPattern;
        }
    }

    return resolved;
}
} // namespace TherionStudio
