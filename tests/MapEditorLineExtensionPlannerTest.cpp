#include "../src/app/text_editor/map_editor/MapEditorLineExtensionPlanner.h"

#include <QPointF>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

MapGeometryFeature::TH2LineVertex makeLineVertex(const QPointF &anchor)
{
    MapGeometryFeature::TH2LineVertex vertex;
    vertex.anchor = anchor;
    vertex.anchorSourceVertexIndex = -1;
    vertex.incomingSourceVertexIndex = -1;
    vertex.outgoingSourceVertexIndex = -1;
    return vertex;
}

MapEditorInteractiveLineDraftVertex makeDraftVertex(const QPointF &anchor)
{
    MapEditorInteractiveLineDraftVertex vertex;
    vertex.anchorSource = anchor;
    vertex.anchorScene = anchor;
    return vertex;
}

int runAppendPlanTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.closed = false;
    lineFeature.lineVertices = {
        makeLineVertex(QPointF(0.0, 0.0)),
        makeLineVertex(QPointF(10.0, 0.0))
    };

    QVector<MapEditorInteractiveLineDraftVertex> draftVertices;
    MapEditorInteractiveLineDraftVertex seed = makeDraftVertex(QPointF(10.0, 0.0));
    seed.outgoingControlSource = QPointF(11.0, 1.0);
    draftVertices.append(seed);

    MapEditorInteractiveLineDraftVertex appended = makeDraftVertex(QPointF(20.0, 0.0));
    appended.incomingControlSource = QPointF(18.0, -1.0);
    appended.outgoingControlSource = QPointF(22.0, 1.0);
    draftVertices.append(appended);

    const MapEditorLineExtensionPlan plan =
        MapEditorLineExtensionPlanner::planEdit(lineFeature, draftVertices, false);

    if (!expect(plan.resolved, "Append extension plan should resolve.")) {
        return 1;
    }
    if (!expect(plan.restoredVertexIndex == 2, "Append extension should restore selection to the new terminal vertex.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.size() == 3, "Append extension should add one vertex.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.at(1).outgoingControl.has_value()
                && plan.editedVertices.at(1).outgoingControl.value() == QPointF(11.0, 1.0),
                "Append extension should transfer seed outgoing control onto the original terminal vertex.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.at(2).anchor == QPointF(20.0, 0.0),
                "Append extension should include appended anchor.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.at(2).incomingControl.has_value()
                && plan.editedVertices.at(2).incomingControl.value() == QPointF(18.0, -1.0),
                "Append extension should preserve appended incoming control.")) {
        return 1;
    }
    return 0;
}

int runPrependPlanTest()
{
    MapGeometryFeature lineFeature;
    lineFeature.closed = false;
    lineFeature.lineVertices = {
        makeLineVertex(QPointF(0.0, 0.0)),
        makeLineVertex(QPointF(10.0, 0.0))
    };

    QVector<MapEditorInteractiveLineDraftVertex> draftVertices;
    MapEditorInteractiveLineDraftVertex seed = makeDraftVertex(QPointF(0.0, 0.0));
    seed.outgoingControlSource = QPointF(-1.0, 1.0);
    draftVertices.append(seed);

    MapEditorInteractiveLineDraftVertex prepended = makeDraftVertex(QPointF(-10.0, 0.0));
    prepended.incomingControlSource = QPointF(-8.0, -1.0);
    prepended.outgoingControlSource = QPointF(-12.0, 1.0);
    draftVertices.append(prepended);

    const MapEditorLineExtensionPlan plan =
        MapEditorLineExtensionPlanner::planEdit(lineFeature, draftVertices, true);

    if (!expect(plan.resolved, "Prepend extension plan should resolve.")) {
        return 1;
    }
    if (!expect(plan.restoredVertexIndex == 0, "Prepend extension should restore selection to the new first vertex.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.size() == 3, "Prepend extension should add one vertex.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.at(0).anchor == QPointF(-10.0, 0.0),
                "Prepend extension should insert prepended anchor before original vertices.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.at(0).incomingControl.has_value()
                && plan.editedVertices.at(0).incomingControl.value() == QPointF(-12.0, 1.0),
                "Prepend extension should map draft outgoing control to incoming control on prepended vertex.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.at(0).outgoingControl.has_value()
                && plan.editedVertices.at(0).outgoingControl.value() == QPointF(-8.0, -1.0),
                "Prepend extension should map draft incoming control to outgoing control on prepended vertex.")) {
        return 1;
    }
    if (!expect(plan.editedVertices.at(1).incomingControl.has_value()
                && plan.editedVertices.at(1).incomingControl.value() == QPointF(-1.0, 1.0),
                "Prepend extension should transfer seed outgoing control onto the original first vertex.")) {
        return 1;
    }
    return 0;
}

int runInvalidPlanInputsTest()
{
    MapGeometryFeature invalidLineFeature;
    invalidLineFeature.lineVertices = {
        makeLineVertex(QPointF(0.0, 0.0))
    };
    QVector<MapEditorInteractiveLineDraftVertex> draftVertices;
    draftVertices.append(makeDraftVertex(QPointF(0.0, 0.0)));
    draftVertices.append(makeDraftVertex(QPointF(1.0, 0.0)));

    const MapEditorLineExtensionPlan invalidFeaturePlan =
        MapEditorLineExtensionPlanner::planEdit(invalidLineFeature, draftVertices, false);
    if (!expect(!invalidFeaturePlan.resolved && !invalidFeaturePlan.errorMessage.isEmpty(),
                "Extension plan should fail for line features with fewer than two vertices.")) {
        return 1;
    }

    MapGeometryFeature lineFeature;
    lineFeature.lineVertices = {
        makeLineVertex(QPointF(0.0, 0.0)),
        makeLineVertex(QPointF(10.0, 0.0))
    };
    QVector<MapEditorInteractiveLineDraftVertex> incompleteDraftVertices;
    incompleteDraftVertices.append(makeDraftVertex(QPointF(10.0, 0.0)));
    const MapEditorLineExtensionPlan incompleteDraftPlan =
        MapEditorLineExtensionPlanner::planEdit(lineFeature, incompleteDraftVertices, false);
    return expect(!incompleteDraftPlan.resolved && !incompleteDraftPlan.errorMessage.isEmpty(),
                  "Extension plan should fail when no new draft vertices were added.")
        ? 0
        : 1;
}
}

int main()
{
    if (const int rc = runAppendPlanTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runPrependPlanTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runInvalidPlanInputsTest(); rc != 0) {
        return rc;
    }
    return 0;
}
