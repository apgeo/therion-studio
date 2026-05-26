#pragma once

#include <QColor>
#include <QString>
#include <QVector>
#include <Qt>

#include <optional>

namespace TherionStudio
{
enum class MapEditorFillPatternKind
{
    None,
    Hatch,
    CrossHatch,
    Dots
};

struct MapEditorAreaFillPatternStyle
{
    MapEditorFillPatternKind kind = MapEditorFillPatternKind::None;
    qreal spacing = 6.0;
    qreal angle = 45.0;
    qreal strokeWidth = 1.0;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    QVector<qreal> dashPattern;
    qreal radius = 1.2;
    std::optional<QColor> dotColor;
    qreal angleJitter = 0.0;
    qreal offsetJitter = 0.0;
    std::optional<int> seed;
};

struct MapEditorPointStyleDefaults
{
    qreal radius = 5.6;
    qreal outlineWidth = 1.1;
    std::optional<QColor> fillColor;
    std::optional<QColor> strokeColor;
};

struct MapEditorLineStyleDefaults
{
    qreal strokeWidth = 3.2;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    QVector<qreal> dashPattern;
};

struct MapEditorAreaStyleDefaults
{
    qreal strokeWidth = 2.0;
    qreal fillOpacity = 0.11;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    std::optional<QColor> fillColor;
    QVector<qreal> dashPattern;
    std::optional<MapEditorAreaFillPatternStyle> fillPattern;
};

struct MapEditorStyleSelector
{
    QString rawType;
    QString subtype;
};

struct MapEditorPointStyleRule
{
    MapEditorStyleSelector selector;
    std::optional<qreal> radius;
    std::optional<qreal> outlineWidth;
    std::optional<QColor> fillColor;
    std::optional<QColor> strokeColor;
};

struct MapEditorLineStyleRule
{
    MapEditorStyleSelector selector;
    std::optional<qreal> strokeWidth;
    std::optional<Qt::PenStyle> penStyle;
    std::optional<QColor> strokeColor;
    std::optional<QVector<qreal>> dashPattern;
};

struct MapEditorAreaStyleRule
{
    MapEditorStyleSelector selector;
    std::optional<qreal> strokeWidth;
    std::optional<qreal> fillOpacity;
    std::optional<Qt::PenStyle> penStyle;
    std::optional<QColor> strokeColor;
    std::optional<QColor> fillColor;
    std::optional<QVector<qreal>> dashPattern;
    std::optional<MapEditorAreaFillPatternStyle> fillPattern;
};

struct MapEditorObjectStyleCatalog
{
    MapEditorPointStyleDefaults point;
    MapEditorLineStyleDefaults line;
    MapEditorAreaStyleDefaults area;
    QVector<MapEditorPointStyleRule> pointStyles;
    QVector<MapEditorLineStyleRule> lineStyles;
    QVector<MapEditorAreaStyleRule> areaStyles;
    bool loadedFromResource = false;
    bool loadedUserOverrides = false;
};

struct MapEditorResolvedPointStyle
{
    qreal radius = 5.6;
    qreal outlineWidth = 1.1;
    std::optional<QColor> fillColor;
    std::optional<QColor> strokeColor;
};

struct MapEditorResolvedLineStyle
{
    qreal strokeWidth = 3.2;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    QVector<qreal> dashPattern;
};

struct MapEditorResolvedAreaStyle
{
    qreal strokeWidth = 2.0;
    qreal fillOpacity = 0.11;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    std::optional<QColor> fillColor;
    QVector<qreal> dashPattern;
    std::optional<MapEditorAreaFillPatternStyle> fillPattern;
};

QString mapEditorUserObjectStylesDirectory();
MapEditorObjectStyleCatalog mapEditorObjectStyleCatalog();
MapEditorResolvedPointStyle resolveMapEditorPointStyle(const MapEditorObjectStyleCatalog &catalog,
                                                       const QString &rawType,
                                                       const QString &subtype = QString());
MapEditorResolvedLineStyle resolveMapEditorLineStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype = QString());
MapEditorResolvedAreaStyle resolveMapEditorAreaStyle(const MapEditorObjectStyleCatalog &catalog,
                                                     const QString &rawType,
                                                     const QString &subtype = QString());
}
