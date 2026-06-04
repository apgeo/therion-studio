#pragma once

#include <QHash>
#include <QLineF>
#include <QPointF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct TherionXviSketchLine
{
    QString colorToken;
    QVector<QPointF> points;
};

struct TherionXviStation
{
    QString name;
    QPointF position;
};

struct TherionXviDocument
{
    QPointF gridOrigin;
    bool hasGridOrigin = false;
    QPointF gridVectorX;
    QPointF gridVectorY;
    int gridCountX = 0;
    int gridCountY = 0;
    bool hasGridDefinition = false;
    QVector<TherionXviStation> stationEntries;
    QHash<QString, QPointF> stations;
    QVector<QLineF> shots;
    QVector<TherionXviSketchLine> sketchLines;
};

bool parseTherionXviDocumentText(const QString &content, TherionXviDocument *document);
bool parseTherionXviDocumentFile(const QString &xviPath, TherionXviDocument *document);
}
