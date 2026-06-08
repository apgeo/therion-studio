#pragma once

#include "MapEditorSceneSupport.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct MapEditorReferencedAreaResolution
{
    QVector<QPointF> vertices;
    QVector<MapGeometryFeature::TH2LineVertex> lineVertices;
};

MapEditorReferencedAreaResolution resolveMapEditorReferencedArea(
    const QStringList &identifiers,
    const QHash<QString, MapGeometryFeature> &lineFeaturesByIdentifier);
}
