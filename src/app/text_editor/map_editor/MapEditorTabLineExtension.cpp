#include "MapEditorTab.h"

#include "MapEditorSourceReferenceResolver.h"
#include "../TextEditorTab.h"

#include <QTimer>

namespace TherionStudio
{
namespace
{
int lineVertexIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        if (lineFeature.lineVertices.at(index).anchorSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

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

bool MapEditorTab::beginLineExtensionFromSelection(int lineNumber, int sourceVertexIndex, bool prepend)
{
    if (textEditor_ == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return false;
    }

    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(textEditor_->text(), lineNumber);
    if (!lineFeature.has_value() || lineFeature->lineVertices.size() < 2) {
        return false;
    }

    const int vertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
    if ((prepend && vertexIndex != 0) || (!prepend && vertexIndex != lineFeature->lineVertices.size() - 1)) {
        return false;
    }

    clearInteractiveDrawSession(false);
    lineExtensionActive_ = true;
    lineExtensionLineNumber_ = lineNumber;
    lineExtensionPrepend_ = prepend;
    interactiveDrawMode_ = InteractiveDrawMode::Line;
    selectModeActive_ = false;

    const MapGeometryFeature::TH2LineVertex &endpoint = lineFeature->lineVertices.at(vertexIndex);
    MapEditorInteractiveLineDraftVertex seed;
    seed.anchorSource = endpoint.anchor;
    seed.anchorScene = scenePointFromSourcePosition(endpoint.anchor);
    interactiveDrawLineVertices_.append(seed);
    updateInteractiveDrawPreview();
    emit modeStatusChanged();
    updateCommandSurfaceState();

    toolbarStatusNote_ = prepend
        ? tr("Extend Before: continue drawing from the first vertex, then press Enter or Complete Draft.")
        : tr("Extend After: continue drawing from the last vertex, then press Enter or Complete Draft.");
    refreshToolbarSummary();
    return true;
}

bool MapEditorTab::commitLineExtensionSession()
{
    if (!lineExtensionActive_) {
        return false;
    }
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Extend line failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return true;
    }
    if (interactiveDrawLineVertices_.size() < 2) {
        toolbarStatusNote_ = tr("Extend line needs at least one new vertex before completion.");
        refreshToolbarSummary();
        return true;
    }

    const QString beforeText = textEditor_->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineExtensionLineNumber_);
    if (!lineFeature.has_value() || lineFeature->lineVertices.size() < 2) {
        toolbarStatusNote_ = tr("Extend line failed: line geometry could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    if (lineExtensionPrepend_) {
        QVector<MapGeometryFeature::TH2LineVertex> prependedVertices;
        prependedVertices.reserve(interactiveDrawLineVertices_.size() - 1);
        for (int index = interactiveDrawLineVertices_.size() - 1; index >= 1; --index) {
            MapGeometryFeature::TH2LineVertex lineVertex = draftVertexToLineVertex(interactiveDrawLineVertices_.at(index));
            lineVertex.incomingControl = interactiveDrawLineVertices_.at(index).outgoingControlSource;
            lineVertex.outgoingControl = interactiveDrawLineVertices_.at(index).incomingControlSource;
            prependedVertices.append(lineVertex);
        }
        if (interactiveDrawLineVertices_.first().outgoingControlSource.has_value()) {
            editedVertices[0].incomingControl = interactiveDrawLineVertices_.first().outgoingControlSource;
        }
        editedVertices = prependedVertices + editedVertices;
    } else {
        if (interactiveDrawLineVertices_.first().outgoingControlSource.has_value()) {
            editedVertices.last().outgoingControl = interactiveDrawLineVertices_.first().outgoingControlSource;
        }
        for (int index = 1; index < interactiveDrawLineVertices_.size(); ++index) {
            editedVertices.append(draftVertexToLineVertex(interactiveDrawLineVertices_.at(index)));
        }
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices, lineFeature->closed);
    QString errorMessage;
    if (!textEditor_->rewriteLineCoordinateRows(lineExtensionLineNumber_, coordinateRows, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Extend line failed.")
            : tr("Extend line failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return true;
    }

    const int extendedLineNumber = lineExtensionLineNumber_;
    const int restoredVertexIndex = lineExtensionPrepend_ ? 0 : editedVertices.size() - 1;
    recordSourceTextSnapshot(tr("Extend Line"), beforeText, textEditor_->text(), extendedLineNumber);
    const std::optional<MapGeometryFeature> refreshedFeature = lineFeatureForLineNumber(textEditor_->text(), extendedLineNumber);
    const int restoredSourceVertexIndex =
        refreshedFeature.has_value() && restoredVertexIndex >= 0 && restoredVertexIndex < refreshedFeature->lineVertices.size()
        ? refreshedFeature->lineVertices.at(restoredVertexIndex).anchorSourceVertexIndex
        : -1;
    lineExtensionActive_ = false;
    lineExtensionLineNumber_ = 0;
    lineExtensionPrepend_ = false;
    clearInteractiveDrawSession(true);
    if (restoredSourceVertexIndex >= 0) {
        restoreLineAnchorSelection(extendedLineNumber, restoredSourceVertexIndex);
        QTimer::singleShot(0, this, [this, extendedLineNumber, restoredSourceVertexIndex]() {
            restoreLineAnchorSelection(extendedLineNumber, restoredSourceVertexIndex);
        });
    }
    toolbarStatusNote_ = tr("Extended line on source line %1.").arg(extendedLineNumber);
    refreshToolbarSummary();
    updateHelpPanel();
    updateCommandSurfaceState();
    return true;
}

}
