#pragma once

#include <QColor>
#include <QVector>
#include <Qt>

#include <optional>

namespace TherionStudio
{
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
    qreal detailWidth = 1.8;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    std::optional<QColor> detailColor;
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
    Qt::BrushStyle fillPattern = Qt::SolidPattern;
    std::optional<QColor> fillPatternColor;
    bool useFillPattern = false;
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
    std::optional<qreal> detailWidth;
    std::optional<Qt::PenStyle> penStyle;
    std::optional<QColor> strokeColor;
    std::optional<QColor> detailColor;
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
    std::optional<Qt::BrushStyle> fillPattern;
    std::optional<QColor> fillPatternColor;
    std::optional<bool> useFillPattern;
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
    qreal detailWidth = 1.8;
    Qt::PenStyle penStyle = Qt::SolidLine;
    std::optional<QColor> strokeColor;
    std::optional<QColor> detailColor;
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
    Qt::BrushStyle fillPattern = Qt::SolidPattern;
    std::optional<QColor> fillPatternColor;
    bool useFillPattern = false;
};

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
