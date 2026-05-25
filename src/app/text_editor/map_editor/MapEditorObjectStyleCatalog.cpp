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

Qt::BrushStyle brushStyleForPatternKind(const QString &kind, qreal angle)
{
    const QString normalized = kind.trimmed().toLower();
    if (normalized == QStringLiteral("dots") || normalized == QStringLiteral("dot")) {
        return Qt::Dense6Pattern;
    }
    if (normalized == QStringLiteral("cross")) {
        return Qt::CrossPattern;
    }
    if (normalized == QStringLiteral("diag-cross")) {
        return Qt::DiagCrossPattern;
    }
    if (normalized == QStringLiteral("hatch")) {
        const qreal normalizedAngle = std::fmod(std::abs(angle), 180.0);
        if (std::abs(normalizedAngle - 45.0) <= 12.0 || std::abs(normalizedAngle - 135.0) <= 12.0) {
            return Qt::BDiagPattern;
        }
        if (std::abs(normalizedAngle - 30.0) <= 15.0 || std::abs(normalizedAngle - 150.0) <= 15.0) {
            return Qt::FDiagPattern;
        }
        return Qt::HorPattern;
    }
    return Qt::SolidPattern;
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

MapEditorStyleSelector readSelector(const QJsonObject &object)
{
    MapEditorStyleSelector selector;
    selector.rawType = normalizedToken(object.value(QStringLiteral("rawType")).toString());
    selector.subtype = normalizedToken(object.value(QStringLiteral("subtype")).toString());
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
        if (const std::optional<qreal> outlineWidth = optionalPositiveNumber(point, "outline_width")) {
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
        if (const std::optional<qreal> detailWidth = optionalPositiveNumber(line, "detail_width")) {
            catalog.line.detailWidth = detailWidth.value();
        }
        if (line.value(QStringLiteral("pen_style")).isString()) {
            catalog.line.penStyle = penStyleFromString(line.value(QStringLiteral("pen_style")).toString());
        }
        catalog.line.strokeColor = optionalColor(line, "stroke_color");
        catalog.line.detailColor = optionalColor(line, "detail_color");
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
        if (area.value(QStringLiteral("pen_style")).isString()) {
            catalog.area.penStyle = penStyleFromString(area.value(QStringLiteral("pen_style")).toString());
        }
        catalog.area.strokeColor = optionalColor(area, "stroke_color");
        catalog.area.fillColor = optionalColor(area, "fill_color");
        if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(area, "dash_pattern")) {
            catalog.area.dashPattern = dashPattern.value();
        }
    }

    const QJsonArray pointStyles = root.value(QStringLiteral("pointStyles")).toArray();
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
        rule.radius = optionalPositiveNumber(object, "pointRadius");
        if (!rule.radius.has_value()) {
            rule.radius = optionalPositiveNumber(object, "radius");
        }
        rule.outlineWidth = optionalPositiveNumber(object, "outlineWidth");
        rule.fillColor = optionalColor(object, "fillColor");
        rule.strokeColor = optionalColor(object, "strokeColor");
        catalog.pointStyles.append(rule);
    }

    const QJsonArray lineStyles = root.value(QStringLiteral("lineStyles")).toArray();
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
        rule.strokeWidth = optionalPositiveNumber(object, "lineWidth");
        if (!rule.strokeWidth.has_value()) {
            rule.strokeWidth = optionalPositiveNumber(object, "strokeWidth");
        }
        rule.detailWidth = optionalPositiveNumber(object, "detailWidth");
        if (object.value(QStringLiteral("penStyle")).isString()) {
            rule.penStyle = penStyleFromString(object.value(QStringLiteral("penStyle")).toString());
        }
        rule.strokeColor = optionalColor(object, "strokeColor");
        rule.detailColor = optionalColor(object, "detailColor");
        if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dashPattern")) {
            rule.dashPattern = dashPattern;
        }
        catalog.lineStyles.append(rule);
    }

    const QJsonArray areaStyles = root.value(QStringLiteral("areaStyles")).toArray();
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

        rule.strokeWidth = optionalPositiveNumber(object, "strokeWidth");
        rule.fillOpacity = optionalZeroToOneNumber(object, "fillOpacity");
        if (object.value(QStringLiteral("penStyle")).isString()) {
            rule.penStyle = penStyleFromString(object.value(QStringLiteral("penStyle")).toString());
        }

        rule.strokeColor = optionalColor(object, "strokeColor");
        rule.fillColor = optionalColor(object, "fillColor");
        if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dashPattern")) {
            rule.dashPattern = dashPattern;
        }

        const QJsonObject fillStyle = object.value(QStringLiteral("fillStyle")).toObject();
        if (!fillStyle.isEmpty()) {
            const QString kind = fillStyle.value(QStringLiteral("kind")).toString();
            const qreal angle = fillStyle.value(QStringLiteral("angle")).toDouble(45.0);
            if (!kind.trimmed().isEmpty()) {
                const Qt::BrushStyle patternStyle = brushStyleForPatternKind(kind, angle);
                if (patternStyle != Qt::SolidPattern || normalizedToken(kind) == QStringLiteral("solid")) {
                    rule.fillPattern = patternStyle;
                    rule.useFillPattern = patternStyle != Qt::SolidPattern;
                }
            }
            if (const std::optional<QColor> color = optionalColor(fillStyle, "color")) {
                rule.fillColor = color;
            }
            if (const std::optional<QColor> backgroundColor = optionalColor(fillStyle, "backgroundColor")) {
                rule.fillColor = backgroundColor;
            }
            if (const std::optional<QColor> patternColor = optionalColor(fillStyle, "strokeColor")) {
                rule.fillPatternColor = patternColor;
            }
            if (const std::optional<QColor> dotColor = optionalColor(fillStyle, "dotColor")) {
                rule.fillPatternColor = dotColor;
            }
        }

        const QJsonObject strokeStyle = object.value(QStringLiteral("strokeStyle")).toObject();
        if (!strokeStyle.isEmpty()) {
            if (const std::optional<qreal> width = optionalPositiveNumber(strokeStyle, "width")) {
                rule.strokeWidth = width;
            }
            if (const std::optional<QColor> color = optionalColor(strokeStyle, "color")) {
                rule.strokeColor = color;
            }
            if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(strokeStyle, "dashPattern")) {
                rule.dashPattern = dashPattern;
            }
        }

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
    resolved.detailWidth = catalog.line.detailWidth;
    resolved.penStyle = catalog.line.penStyle;
    resolved.strokeColor = catalog.line.strokeColor;
    resolved.detailColor = catalog.line.detailColor;
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
        if (rule.detailWidth.has_value()) {
            resolved.detailWidth = rule.detailWidth.value();
        }
        if (rule.penStyle.has_value()) {
            resolved.penStyle = rule.penStyle.value();
        }
        if (rule.strokeColor.has_value()) {
            resolved.strokeColor = rule.strokeColor;
        }
        if (rule.detailColor.has_value()) {
            resolved.detailColor = rule.detailColor;
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
    resolved.fillPatternColor = catalog.area.fillPatternColor;
    resolved.useFillPattern = catalog.area.useFillPattern;

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
            resolved.fillPattern = rule.fillPattern.value();
        }
        if (rule.fillPatternColor.has_value()) {
            resolved.fillPatternColor = rule.fillPatternColor;
        }
        if (rule.useFillPattern.has_value()) {
            resolved.useFillPattern = rule.useFillPattern.value();
        }
    }

    return resolved;
}
} // namespace TherionStudio
