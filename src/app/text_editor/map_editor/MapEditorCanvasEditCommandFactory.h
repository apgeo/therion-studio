#pragma once

#include "MapEditorSceneSupport.h"

#include <QPointF>
#include <QVector>

namespace TherionStudio
{
struct MapLineAreaVertexSecondaryMove
{
    int vertexIndex = -1;
    QPointF oldPoint;
    QPointF newPoint;
};

struct MapLineAreaVertexMoveSet
{
    QVector<MapLineAreaVertexSecondaryMove> secondaryMoves;
};

MapLineAreaVertexMoveSet coupledLineVertexMoveSet(const MapGeometryFeature &lineFeature,
                                                  int sourceVertexIndex,
                                                  const QPointF &oldPoint,
                                                  const QPointF &newPoint);
}
