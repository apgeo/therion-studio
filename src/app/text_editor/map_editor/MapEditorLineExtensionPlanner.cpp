#include "MapEditorLineExtensionPlanner.h"

#include <QCoreApplication>

namespace TherionStudio
{
namespace
{
MapGeometryFeature::TH2LineVertex draftVertexToLineVertex(const MapEditorInteractiveLineDraftVertex &draftVertex)
{
    MapGeometryFeature::TH2LineVertex lineVertex;
    lineVertex.anchor = draftVertex.anchorSource;
    lineVertex.incomingControl = draftVertex.incomingControlSource;
    lineVertex.outgoingControl = draftVertex.outgoingControlSource;
    lineVertex.anchorSourceVertexIndex = -1;
    lineVertex.incomingSourceVertexIndex = -1;
    lineVertex.outgoingSourceVertexIndex = -1;
    return lineVertex;
}
}

MapEditorLineExtensionPlan MapEditorLineExtensionPlanner::planEdit(
    const MapGeometryFeature &lineFeature,
    const QVector<MapEditorInteractiveLineDraftVertex> &draftVertices,
    bool prepend)
{
    MapEditorLineExtensionPlan plan;
    if (lineFeature.lineVertices.size() < 2) {
        plan.errorMessage = QCoreApplication::translate("TherionStudio::MapEditorLineExtensionPlanner",
                                                        "Extend line failed: line geometry could not be resolved.");
        return plan;
    }
    if (draftVertices.size() < 2) {
        plan.errorMessage = QCoreApplication::translate(
            "TherionStudio::MapEditorLineExtensionPlanner",
            "Extend line needs at least one new vertex before completion.");
        return plan;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature.lineVertices;
    if (prepend) {
        QVector<MapGeometryFeature::TH2LineVertex> prependedVertices;
        prependedVertices.reserve(draftVertices.size() - 1);
        for (int index = draftVertices.size() - 1; index >= 1; --index) {
            MapGeometryFeature::TH2LineVertex lineVertex = draftVertexToLineVertex(draftVertices.at(index));
            lineVertex.incomingControl = draftVertices.at(index).outgoingControlSource;
            lineVertex.outgoingControl = draftVertices.at(index).incomingControlSource;
            prependedVertices.append(lineVertex);
        }
        if (draftVertices.first().outgoingControlSource.has_value()) {
            editedVertices[0].incomingControl = draftVertices.first().outgoingControlSource;
        }
        editedVertices = prependedVertices + editedVertices;
        plan.restoredVertexIndex = 0;
    } else {
        if (draftVertices.first().outgoingControlSource.has_value()) {
            editedVertices.last().outgoingControl = draftVertices.first().outgoingControlSource;
        }
        for (int index = 1; index < draftVertices.size(); ++index) {
            editedVertices.append(draftVertexToLineVertex(draftVertices.at(index)));
        }
        plan.restoredVertexIndex = editedVertices.size() - 1;
    }

    plan.editedVertices = std::move(editedVertices);
    plan.resolved = true;
    return plan;
}
}
