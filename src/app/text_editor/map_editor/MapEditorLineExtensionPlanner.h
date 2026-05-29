#pragma once

#include "MapEditorInteractiveDrawLogic.h"
#include "MapEditorSceneSupport.h"

#include <QString>
#include <QVector>

namespace TherionStudio
{
struct MapEditorLineExtensionPlan
{
    bool resolved = false;
    QString errorMessage;
    QVector<MapGeometryFeature::TH2LineVertex> editedVertices;
    int restoredVertexIndex = -1;
};

class MapEditorLineExtensionPlanner final
{
public:
    [[nodiscard]] static MapEditorLineExtensionPlan planEdit(const MapGeometryFeature &lineFeature,
                                                             const QVector<MapEditorInteractiveLineDraftVertex> &draftVertices,
                                                             bool prepend);
};
}
