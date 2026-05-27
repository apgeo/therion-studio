#include "MapEditorObjectStyleCatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <algorithm>
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

std::optional<qreal> optionalFiniteNumber(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isDouble()) {
        return std::nullopt;
    }
    const qreal number = value.toDouble();
    if (!std::isfinite(number)) {
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

std::optional<bool> optionalBool(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isBool()) {
        return std::nullopt;
    }
    return value.toBool();
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

std::optional<QString> optionalFieldName(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isString()) {
        return std::nullopt;
    }

    QString field = value.toString().trimmed().toLower();
    while (field.startsWith(QLatin1Char('-'))) {
        field.remove(0, 1);
    }
    if (field.isEmpty()) {
        return std::nullopt;
    }
    return field;
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

std::optional<QVector<qreal>> optionalNumberArray(const QJsonObject &object, const char *key)
{
    const QJsonValue value = object.value(QLatin1String(key));
    if (!value.isArray()) {
        return std::nullopt;
    }

    const QJsonArray array = value.toArray();
    QVector<qreal> numbers;
    numbers.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isDouble()) {
            return std::nullopt;
        }
        const qreal number = entry.toDouble();
        if (!std::isfinite(number)) {
            return std::nullopt;
        }
        numbers.append(number);
    }

    return numbers;
}

QString normalizedToken(const QString &value)
{
    return value.trimmed().toLower();
}

std::optional<MapEditorLineDecorationKind> lineDecorationKindFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("offset_stroke") || normalized == QStringLiteral("offset-stroke")) {
        return MapEditorLineDecorationKind::OffsetStroke;
    }
    if (normalized == QStringLiteral("parallel")) {
        return MapEditorLineDecorationKind::Parallel;
    }
    if (normalized == QStringLiteral("ticks")) {
        return MapEditorLineDecorationKind::Ticks;
    }
    if (normalized == QStringLiteral("rungs")) {
        return MapEditorLineDecorationKind::Rungs;
    }
    if (normalized == QStringLiteral("teeth")) {
        return MapEditorLineDecorationKind::Teeth;
    }
    if (normalized == QStringLiteral("symbols") || normalized == QStringLiteral("dots")) {
        return MapEditorLineDecorationKind::Symbols;
    }
    return std::nullopt;
}

MapEditorLineDecorationSide lineDecorationSideFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("left")) {
        return MapEditorLineDecorationSide::Left;
    }
    if (normalized == QStringLiteral("right")) {
        return MapEditorLineDecorationSide::Right;
    }
    return MapEditorLineDecorationSide::Center;
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

std::optional<MapEditorPointSymbol> pointSymbolFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("circle")) {
        return MapEditorPointSymbol::Circle;
    }
    if (normalized == QStringLiteral("square")) {
        return MapEditorPointSymbol::Square;
    }
    if (normalized == QStringLiteral("diamond")) {
        return MapEditorPointSymbol::Diamond;
    }
    if (normalized == QStringLiteral("triangle")) {
        return MapEditorPointSymbol::Triangle;
    }
    if (normalized == QStringLiteral("star")) {
        return MapEditorPointSymbol::Star;
    }
    if (normalized == QStringLiteral("asterisk")) {
        return MapEditorPointSymbol::Asterisk;
    }
    if (normalized == QStringLiteral("plus")) {
        return MapEditorPointSymbol::Plus;
    }
    if (normalized == QStringLiteral("x")) {
        return MapEditorPointSymbol::X;
    }
    return std::nullopt;
}

std::optional<MapEditorPointSymbol> dotPatternSymbolFromString(const QString &value)
{
    const QString normalized = normalizedToken(value);
    if (normalized == QStringLiteral("oval")) {
        return MapEditorPointSymbol::Oval;
    }

    return pointSymbolFromString(value);
}

std::optional<MapEditorLineDecorationStyle> readLineDecoration(const QJsonObject &object)
{
    if (object.isEmpty()) {
        return std::nullopt;
    }

    const std::optional<MapEditorLineDecorationKind> kind =
        lineDecorationKindFromString(object.value(QStringLiteral("kind")).toString());
    if (!kind.has_value()) {
        return std::nullopt;
    }

    MapEditorLineDecorationStyle decoration;
    decoration.kind = kind.value();
    decoration.side = lineDecorationSideFromString(object.value(QStringLiteral("side")).toString());

    if (decoration.kind == MapEditorLineDecorationKind::Teeth
        && object.value(QStringLiteral("side")).toString().trimmed().isEmpty()) {
        decoration.side = MapEditorLineDecorationSide::Right;
    }

    if (const std::optional<qreal> spacing = optionalPositiveNumber(object, "spacing")) {
        decoration.spacing = spacing.value();
    }
    if (const std::optional<qreal> offset = optionalFiniteNumber(object, "offset")) {
        decoration.offset = offset.value();
    }
    if (const std::optional<QVector<qreal>> offsets = optionalNumberArray(object, "offsets")) {
        decoration.offsets = offsets.value();
    }
    decoration.fromOffset = optionalFiniteNumber(object, "from_offset");
    decoration.toOffset = optionalFiniteNumber(object, "to_offset");
    if (const std::optional<qreal> size = optionalPositiveNumber(object, "size")) {
        decoration.size = size.value();
    }
    if (const std::optional<qreal> length = optionalPositiveNumber(object, "length")) {
        decoration.length = length.value();
    }
    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(object, "stroke_width")) {
        decoration.strokeWidth = strokeWidth.value();
    }
    if (object.value(QStringLiteral("stroke_style")).isString()) {
        decoration.strokeStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
    }
    decoration.strokeColor = optionalColor(object, "stroke_color");
    decoration.fillColor = optionalColor(object, "fill_color");
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
        decoration.dashPattern = dashPattern.value();
    }
    if (object.value(QStringLiteral("symbol")).isString()) {
        if (const std::optional<MapEditorPointSymbol> symbol =
                dotPatternSymbolFromString(object.value(QStringLiteral("symbol")).toString())) {
            decoration.symbol = symbol.value();
        }
    }
    if (const std::optional<qreal> angle = optionalFiniteNumber(object, "angle")) {
        decoration.angle = angle.value();
    }
    if (const std::optional<qreal> angleJitter = optionalNonNegativeNumber(object, "angle_jitter")) {
        decoration.angleJitter = angleJitter.value();
    }
    if (const std::optional<qreal> sizeJitter = optionalNonNegativeNumber(object, "size_jitter")) {
        decoration.sizeJitter = sizeJitter.value();
    }
    if (const std::optional<qreal> offsetJitter = optionalNonNegativeNumber(object, "offset_jitter")) {
        decoration.offsetJitter = offsetJitter.value();
    }
    if (const std::optional<int> seed = optionalInteger(object, "seed")) {
        decoration.seed = seed;
    }

    return decoration;
}

std::optional<QVector<MapEditorLineDecorationStyle>> readLineDecorations(const QJsonObject &object)
{
    const QJsonValue value = object.value(QStringLiteral("decorations"));
    if (!value.isArray()) {
        return std::nullopt;
    }

    QVector<MapEditorLineDecorationStyle> decorations;
    const QJsonArray array = value.toArray();
    decorations.reserve(array.size());
    for (const QJsonValue &entry : array) {
        if (!entry.isObject()) {
            continue;
        }
        if (const std::optional<MapEditorLineDecorationStyle> decoration =
                readLineDecoration(entry.toObject())) {
            decorations.append(decoration.value());
        }
    }
    return decorations;
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
    if (const std::optional<qreal> size = optionalPositiveNumber(object, "size")) {
        pattern.size = size.value();
    }
    if (const std::optional<qreal> sizeJitter = optionalNonNegativeNumber(object, "size_jitter")) {
        pattern.sizeJitter = sizeJitter.value();
    }
    if (pattern.kind == MapEditorFillPatternKind::Dots
        && object.value(QStringLiteral("symbol")).isString()) {
        if (const std::optional<MapEditorPointSymbol> symbol =
                dotPatternSymbolFromString(object.value(QStringLiteral("symbol")).toString())) {
            pattern.symbol = symbol.value();
        }
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

void applyPointDefaults(MapEditorObjectStyleCatalog *catalog, const QJsonObject &point)
{
    if (catalog == nullptr || point.isEmpty()) {
        return;
    }

    if (point.value(QStringLiteral("symbol")).isString()) {
        if (const std::optional<MapEditorPointSymbol> symbol =
                pointSymbolFromString(point.value(QStringLiteral("symbol")).toString())) {
            catalog->point.symbol = symbol.value();
        }
    }
    if (const std::optional<qreal> size = optionalPositiveNumber(point, "size")) {
        catalog->point.size = size.value();
    }
    if (const std::optional<qreal> outlineWidth = optionalPositiveNumber(point, "stroke_width")) {
        catalog->point.outlineWidth = outlineWidth.value();
    }
    if (const std::optional<QColor> fillColor = optionalColor(point, "fill_color")) {
        catalog->point.fillColor = fillColor;
    }
    if (const std::optional<QColor> strokeColor = optionalColor(point, "stroke_color")) {
        catalog->point.strokeColor = strokeColor;
    }
    if (const std::optional<QString> labelField = optionalFieldName(point, "label_field")) {
        catalog->point.labelField = labelField;
    }
}

void applyLineDefaults(MapEditorObjectStyleCatalog *catalog, const QJsonObject &line)
{
    if (catalog == nullptr || line.isEmpty()) {
        return;
    }

    if (const std::optional<bool> strokeVisible = optionalBool(line, "stroke_visible")) {
        catalog->line.strokeVisible = strokeVisible.value();
    }
    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(line, "stroke_width")) {
        catalog->line.strokeWidth = strokeWidth.value();
    }
    if (line.value(QStringLiteral("stroke_style")).isString()) {
        catalog->line.penStyle = penStyleFromString(line.value(QStringLiteral("stroke_style")).toString());
    }
    if (const std::optional<QColor> strokeColor = optionalColor(line, "stroke_color")) {
        catalog->line.strokeColor = strokeColor;
    }
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(line, "dash_pattern")) {
        catalog->line.dashPattern = dashPattern.value();
    }
    if (const std::optional<QVector<MapEditorLineDecorationStyle>> decorations = readLineDecorations(line)) {
        catalog->line.decorations = decorations.value();
    }
}

void applyAreaDefaults(MapEditorObjectStyleCatalog *catalog, const QJsonObject &area)
{
    if (catalog == nullptr || area.isEmpty()) {
        return;
    }

    if (const std::optional<qreal> strokeWidth = optionalPositiveNumber(area, "stroke_width")) {
        catalog->area.strokeWidth = strokeWidth.value();
    }
    if (const std::optional<qreal> fillOpacity = optionalZeroToOneNumber(area, "fill_opacity")) {
        catalog->area.fillOpacity = fillOpacity.value();
    }
    if (area.value(QStringLiteral("stroke_style")).isString()) {
        catalog->area.penStyle = penStyleFromString(area.value(QStringLiteral("stroke_style")).toString());
    }
    if (const std::optional<QColor> strokeColor = optionalColor(area, "stroke_color")) {
        catalog->area.strokeColor = strokeColor;
    }
    if (const std::optional<QColor> fillColor = optionalColor(area, "fill_color")) {
        catalog->area.fillColor = fillColor;
    }
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(area, "dash_pattern")) {
        catalog->area.dashPattern = dashPattern.value();
    }
    if (area.contains(QStringLiteral("fill_pattern"))) {
        catalog->area.fillPattern = readAreaFillPattern(area.value(QStringLiteral("fill_pattern")).toObject());
    }
}

std::optional<MapEditorPointStyleRule> readPointStyleRule(const QJsonObject &object)
{
    MapEditorPointStyleRule rule;
    rule.selector = readSelector(object);
    if (rule.selector.rawType.isEmpty()) {
        return std::nullopt;
    }

    if (object.value(QStringLiteral("symbol")).isString()) {
        rule.symbol = pointSymbolFromString(object.value(QStringLiteral("symbol")).toString());
    }
    rule.size = optionalPositiveNumber(object, "size");
    rule.outlineWidth = optionalPositiveNumber(object, "stroke_width");
    rule.fillColor = optionalColor(object, "fill_color");
    rule.strokeColor = optionalColor(object, "stroke_color");
    rule.labelField = optionalFieldName(object, "label_field");
    return rule;
}

std::optional<MapEditorLineStyleRule> readLineStyleRule(const QJsonObject &object)
{
    MapEditorLineStyleRule rule;
    rule.selector = readSelector(object);
    if (rule.selector.rawType.isEmpty()) {
        return std::nullopt;
    }

    rule.strokeVisible = optionalBool(object, "stroke_visible");
    rule.strokeWidth = optionalPositiveNumber(object, "stroke_width");
    if (object.value(QStringLiteral("stroke_style")).isString()) {
        rule.penStyle = penStyleFromString(object.value(QStringLiteral("stroke_style")).toString());
    }
    rule.strokeColor = optionalColor(object, "stroke_color");
    if (const std::optional<QVector<qreal>> dashPattern = optionalDashPattern(object, "dash_pattern")) {
        rule.dashPattern = dashPattern;
    }
    rule.decorations = readLineDecorations(object);
    return rule;
}

std::optional<MapEditorAreaStyleRule> readAreaStyleRule(const QJsonObject &object)
{
    MapEditorAreaStyleRule rule;
    rule.selector = readSelector(object);
    if (rule.selector.rawType.isEmpty()) {
        return std::nullopt;
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
    return rule;
}

std::optional<QJsonObject> jsonObjectFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        return std::nullopt;
    }
    return json.object();
}

bool applySplitStyleFile(MapEditorObjectStyleCatalog *catalog, const QFileInfo &fileInfo)
{
    if (catalog == nullptr || !fileInfo.isFile()) {
        return false;
    }

    const std::optional<QJsonObject> jsonObject = jsonObjectFromFile(fileInfo.absoluteFilePath());
    if (!jsonObject.has_value()) {
        return false;
    }

    const QStringList nameParts =
        fileInfo.completeBaseName().split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (nameParts.isEmpty()) {
        return false;
    }

    const QString kind = normalizedToken(nameParts.at(0));
    if (kind != QStringLiteral("point") && kind != QStringLiteral("line") && kind != QStringLiteral("area")) {
        return false;
    }

    if (nameParts.size() == 1) {
        if (kind == QStringLiteral("point")) {
            applyPointDefaults(catalog, jsonObject.value());
        } else if (kind == QStringLiteral("line")) {
            applyLineDefaults(catalog, jsonObject.value());
        } else {
            applyAreaDefaults(catalog, jsonObject.value());
        }
        return true;
    }

    QJsonObject object = jsonObject.value();
    object.insert(QStringLiteral("raw_type"), normalizedToken(nameParts.at(1)));
    if (nameParts.size() > 2) {
        object.insert(QStringLiteral("raw_subtype"), normalizedToken(nameParts.mid(2).join(QLatin1Char('.'))));
    }

    if (kind == QStringLiteral("point")) {
        if (const std::optional<MapEditorPointStyleRule> rule = readPointStyleRule(object)) {
            catalog->pointStyles.append(rule.value());
            return true;
        }
    } else if (kind == QStringLiteral("line")) {
        if (const std::optional<MapEditorLineStyleRule> rule = readLineStyleRule(object)) {
            catalog->lineStyles.append(rule.value());
            return true;
        }
    } else if (const std::optional<MapEditorAreaStyleRule> rule = readAreaStyleRule(object)) {
        catalog->areaStyles.append(rule.value());
        return true;
    }

    return false;
}

int splitStyleOverrideScopeRank(const QFileInfo &fileInfo)
{
    const QStringList nameParts =
        fileInfo.completeBaseName().split(QLatin1Char('.'), Qt::SkipEmptyParts);
    if (nameParts.size() <= 1) {
        return 0;
    }
    if (nameParts.size() == 2) {
        return 1;
    }
    return 2;
}

bool applySplitStyleDirectory(MapEditorObjectStyleCatalog *catalog, const QString &directoryPath)
{
    if (catalog == nullptr || directoryPath.isEmpty()) {
        return false;
    }

    const QDir directory(directoryPath);
    if (!directory.exists()) {
        return false;
    }

    bool loaded = false;
    QFileInfoList files = directory.entryInfoList({QStringLiteral("*.json")},
                                                  QDir::Files | QDir::Readable,
                                                  QDir::Name);
    std::sort(files.begin(), files.end(), [](const QFileInfo &left, const QFileInfo &right) {
        const int leftRank = splitStyleOverrideScopeRank(left);
        const int rightRank = splitStyleOverrideScopeRank(right);
        if (leftRank != rightRank) {
            return leftRank < rightRank;
        }
        return left.fileName() < right.fileName();
    });
    for (const QFileInfo &fileInfo : files) {
        loaded = applySplitStyleFile(catalog, fileInfo) || loaded;
    }
    return loaded;
}

bool applyUserStyleOverrides(MapEditorObjectStyleCatalog *catalog)
{
    return applySplitStyleDirectory(catalog, mapEditorUserObjectStylesDirectory());
}

MapEditorObjectStyleCatalog loadMapEditorObjectStyleCatalogFromResource()
{
    MapEditorObjectStyleCatalog catalog;

    catalog.loadedFromResource =
        applySplitStyleDirectory(&catalog, QStringLiteral(":/resources/map_object_styles"));
    catalog.loadedUserOverrides = applyUserStyleOverrides(&catalog);
    return catalog;
}
} // namespace

QString mapEditorUserObjectStylesDirectory()
{
    const QString environmentOverride = QProcessEnvironment::systemEnvironment()
        .value(QStringLiteral("THERION_STUDIO_MAP_OBJECT_STYLES_DIR"))
        .trimmed();
    if (!environmentOverride.isEmpty()) {
        return environmentOverride;
    }

    const QString appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataLocation.isEmpty()
        ? QString()
        : QDir(appDataLocation).filePath(QStringLiteral("map_object_styles"));
}

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
    resolved.symbol = catalog.point.symbol;
    resolved.size = catalog.point.size;
    resolved.outlineWidth = catalog.point.outlineWidth;
    resolved.fillColor = catalog.point.fillColor;
    resolved.strokeColor = catalog.point.strokeColor;
    resolved.labelField = catalog.point.labelField;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorPointStyleRule &rule : catalog.pointStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.symbol.has_value()) {
            resolved.symbol = rule.symbol.value();
        }
        if (rule.size.has_value()) {
            resolved.size = rule.size.value();
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
        if (rule.labelField.has_value()) {
            resolved.labelField = rule.labelField;
        }
    }

    return resolved;
}

MapEditorResolvedLineStyle resolveMapEditorLineStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype)
{
    MapEditorResolvedLineStyle resolved;
    resolved.strokeVisible = catalog.line.strokeVisible;
    resolved.strokeWidth = catalog.line.strokeWidth;
    resolved.penStyle = catalog.line.penStyle;
    resolved.strokeColor = catalog.line.strokeColor;
    resolved.dashPattern = catalog.line.dashPattern;
    resolved.decorations = catalog.line.decorations;

    const QString normalizedRawType = normalizedToken(rawType);
    const QString normalizedSubtype = normalizedToken(subtype);
    for (const MapEditorLineStyleRule &rule : catalog.lineStyles) {
        if (!selectorMatches(rule.selector, normalizedRawType, normalizedSubtype)) {
            continue;
        }
        if (rule.strokeVisible.has_value()) {
            resolved.strokeVisible = rule.strokeVisible.value();
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
        if (rule.decorations.has_value()) {
            resolved.decorations = rule.decorations.value();
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
