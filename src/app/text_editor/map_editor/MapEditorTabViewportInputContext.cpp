#include "MapEditorTab.h"

#include "MapEditorViewportInputContext.h"

namespace TherionStudio
{
namespace
{
MapEditorInteractiveDrawMode toViewportInteractiveDrawMode(MapEditorTab::InteractiveDrawMode mode)
{
    switch (mode) {
    case MapEditorTab::InteractiveDrawMode::None:
        return MapEditorInteractiveDrawMode::None;
    case MapEditorTab::InteractiveDrawMode::Point:
        return MapEditorInteractiveDrawMode::Point;
    case MapEditorTab::InteractiveDrawMode::Line:
        return MapEditorInteractiveDrawMode::Line;
    case MapEditorTab::InteractiveDrawMode::Freehand:
        return MapEditorInteractiveDrawMode::Freehand;
    case MapEditorTab::InteractiveDrawMode::Area:
        return MapEditorInteractiveDrawMode::Area;
    }
    return MapEditorInteractiveDrawMode::None;
}
}

MapEditorViewportInputContext MapEditorTab::viewportInputContext()
{
    return MapEditorViewportInputContext{
        .textEditor = textEditor_,
        .scene = mapScene_,
        .view = mapView_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .touchFriendlyControlsEnabled = &touchFriendlyControlsEnabled_,
        .selectModeActive = &selectModeActive_,
        .autoFitEnabled = &autoFitEnabled_,
        .fitBackgroundRequested = &fitBackgroundRequested_,
        .mapPanActive = &mapPanActive_,
        .mapPanLastPosition = &mapPanLastPosition_,
        .primaryPointerInteractionActive = &primaryPointerInteractionActive_,
        .touchPanCandidate = &touchPanCandidate_,
        .touchPanActive = &touchPanActive_,
        .touchPanStartPosition = &touchPanStartPosition_,
        .touchPanLastPosition = &touchPanLastPosition_,
        .lastTabletInteractionUtc = &lastTabletInteractionUtc_,
        .nativeZoomGestureActive = &nativeZoomGestureActive_,
        .lastNativeZoomGestureUtc = &lastNativeZoomGestureUtc_,
        .pendingClickSelection = &pendingMapClickSelection_,
        .pendingClickScenePosition = &pendingMapClickScenePosition_,
        .pendingClickElapsed = &pendingMapClickElapsed_,
        .pendingClickLineNumber = &pendingMapClickLineNumber_,
        .pendingClickSourceVertexIndex = &pendingMapClickSourceVertexIndex_,
        .pendingClickGeometryKind = &pendingMapClickGeometryKind_,
        .interactiveDrawSourceVertices = &interactiveDrawSourceVertices_,
        .interactiveDrawSceneVertices = &interactiveDrawSceneVertices_,
        .interactiveDrawLineVertices = &interactiveDrawLineVertices_,
        .interactiveDrawStrokeActive = &interactiveDrawStrokeActive_,
        .interactiveDrawAnchorPressActive = &interactiveDrawAnchorPressActive_,
        .interactiveDrawAnchorPressScenePoint = &interactiveDrawAnchorPressScenePoint_,
        .interactiveDrawAnchorDragActive = &interactiveDrawAnchorDragActive_,
        .interactiveDrawAnchorDragScenePoint = &interactiveDrawAnchorDragScenePoint_,
        .interactiveDrawControlDragActive = &interactiveDrawControlDragActive_,
        .interactiveDrawControlDragHandle = &interactiveDrawControlDragHandle_,
        .interactiveDrawHoverActive = &interactiveDrawHoverActive_,
        .interactiveDrawHoverScenePoint = &interactiveDrawHoverScenePoint_,
        .drawMode = [this]() {
            return toViewportInteractiveDrawMode(interactiveDrawMode_);
        },
        .translate = [this](const char *text) {
            return tr(text);
        },
        .clearInteractiveDrawSession = [this](bool clearMode) {
            clearInteractiveDrawSession(clearMode);
        },
        .sourcePointFromScenePosition = [this](const QPointF &scenePosition) {
            return sourcePointFromScenePosition(scenePosition);
        },
        .updateInteractiveDrawPreview = [this]() {
            updateInteractiveDrawPreview();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .interactiveLineControlAt = [this](const QPointF &scenePosition, qreal sceneRadius) {
            return interactiveLineControlAt(scenePosition, sceneRadius);
        },
        .setInteractiveLineControlScenePoint = [this](const MapEditorInteractiveLineControlHandleRef &handle,
                                                      const QPointF &scenePoint) {
            return setInteractiveLineControlScenePoint(handle, scenePoint);
        },
        .handleInteractiveDrawClick = [this](const QPointF &scenePosition) {
            return handleInteractiveDrawClick(scenePosition);
        },
        .captureInteractiveLineAnchor = [this](const QPointF &anchorScenePoint,
                                               const std::optional<QPointF> &dragScenePoint) {
            captureInteractiveLineAnchor(anchorScenePoint, dragScenePoint);
        },
        .commitInteractiveDrawVertices = [this](const QString &geometryKind,
                                                const QVector<QPointF> &vertices,
                                                const QString &successLabel) {
            return commitInteractiveDrawVertices(geometryKind, vertices, successLabel);
        },
        .commitInteractiveDrawSession = [this](bool closeLineDraft) {
            return commitInteractiveDrawSession(closeLineDraft);
        },
        .updateHelpPanel = [this]() {
            updateHelpPanel();
        },
        .syncZoomFactorFromView = [this]() {
            syncZoomFactorFromView();
        },
        .applyZoomAtViewportPosition = [this](qreal factor, const QPointF &viewportPosition) {
            applyZoomAtViewportPosition(factor, viewportPosition);
        },
        .fitMapToView = [this](bool includeBackgroundImages) {
            fitMapToView(includeBackgroundImages);
        },
        .insertLineVertexFromSelection = [this]() {
            return insertLineVertexFromSelection(false);
        },
        .removeLineVertexFromSelection = [this]() {
            return removeLineVertexFromSelection();
        },
        .toggleLineVertexSmoothFromSelection = [this]() {
            return toggleLineVertexSmoothFromSelection();
        },
    };
}
}
