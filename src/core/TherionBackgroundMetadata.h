#pragma once

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct TherionBackgroundReference
{
    QString absolutePath;
    QPointF basePosition;
    bool hasBasePosition = false;
    bool visible = true;
    bool hasVisibility = false;
    qreal imageScale = 1.0;
    bool hasImageScale = false;
    QString rootStationName;
    bool metadataTopEdgeAnchor = false;
    bool xviReference = false;
    int lineNumber = 0;
};

struct TherionAreaAdjust
{
    QRectF modelRect;
    bool valid = false;
};

QVector<TherionBackgroundReference> parseTherionBackgroundReferences(const QString &documentText,
                                                                     const QString &documentPath);
TherionAreaAdjust parseTherionAreaAdjust(const QString &documentText);
}
