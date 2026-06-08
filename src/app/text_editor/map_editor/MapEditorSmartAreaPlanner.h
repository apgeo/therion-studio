#pragma once

#include "MapEditorSceneSupport.h"

#include <QPointF>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct MapEditorSmartAreaBoundaryLine
{
    int lineNumber = 0;
    QString identifier;
};

struct MapEditorSmartAreaCandidate
{
    int scrapLineNumber = 0;
    QVector<QPointF> vertices;
    QVector<MapEditorSmartAreaBoundaryLine> boundaryLines;
    qreal area = 0.0;
};

QVector<MapEditorSmartAreaCandidate> mapEditorSmartAreaCandidatesAt(
    const QVector<MapGeometryFeature> &geometryFeatures,
    const QPointF &sourcePoint);
}
