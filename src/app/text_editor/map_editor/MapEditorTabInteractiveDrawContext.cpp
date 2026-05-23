#include "MapEditorTab.h"

#include "MapEditorInteractiveDrawContext.h"
#include "MapEditorInteractiveDrawController.h"
#include "../TextEditorTab.h"

namespace TherionStudio
{
namespace
{
MapEditorInteractiveDrawMode toInteractiveDrawControllerMode(MapEditorTab::InteractiveDrawMode mode)
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

MapEditorTab::InteractiveDrawMode toTabInteractiveDrawMode(MapEditorInteractiveDrawMode mode)
{
    switch (mode) {
    case MapEditorInteractiveDrawMode::None:
        return MapEditorTab::InteractiveDrawMode::None;
    case MapEditorInteractiveDrawMode::Point:
        return MapEditorTab::InteractiveDrawMode::Point;
    case MapEditorInteractiveDrawMode::Line:
        return MapEditorTab::InteractiveDrawMode::Line;
    case MapEditorInteractiveDrawMode::Freehand:
        return MapEditorTab::InteractiveDrawMode::Freehand;
    case MapEditorInteractiveDrawMode::Area:
        return MapEditorTab::InteractiveDrawMode::Area;
    }
    return MapEditorTab::InteractiveDrawMode::None;
}
}

MapEditorInteractiveDrawContext MapEditorTab::interactiveDrawContext()
{
    return MapEditorInteractiveDrawContext{
        .textEditor = textEditor_,
        .scene = mapScene_,
        .view = mapView_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .selectModeActive = &selectModeActive_,
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .panActive = &mapPanActive_,
        .sourceVertices = &interactiveDrawSourceVertices_,
        .sceneVertices = &interactiveDrawSceneVertices_,
        .lineVertices = &interactiveDrawLineVertices_,
        .strokeActive = &interactiveDrawStrokeActive_,
        .anchorPressActive = &interactiveDrawAnchorPressActive_,
        .anchorPressScenePoint = &interactiveDrawAnchorPressScenePoint_,
        .anchorDragActive = &interactiveDrawAnchorDragActive_,
        .anchorDragScenePoint = &interactiveDrawAnchorDragScenePoint_,
        .controlDragActive = &interactiveDrawControlDragActive_,
        .hoverActive = &interactiveDrawHoverActive_,
        .hoverScenePoint = &interactiveDrawHoverScenePoint_,
        .previewPath = &interactiveDrawPreviewPath_,
        .previewMarkers = &interactiveDrawPreviewMarkers_,
        .drawMode = [this]() {
            return toInteractiveDrawControllerMode(interactiveDrawMode_);
        },
        .setDrawMode = [this](MapEditorInteractiveDrawMode mode) {
            interactiveDrawMode_ = toTabInteractiveDrawMode(mode);
        },
        .translate = [this](const char *text) {
            return tr(text);
        },
        .emitModeStatusChanged = [this]() {
            emit modeStatusChanged();
        },
        .sourcePointFromScenePosition = [this](const QPointF &scenePosition) {
            return sourcePointFromScenePosition(scenePosition);
        },
        .recordSourceTextSnapshot = [this](const QString &label,
                                           const QString &beforeText,
                                           const QString &afterText,
                                           int insertedLineNumber) {
            recordSourceTextSnapshot(label, beforeText, afterText, insertedLineNumber);
        },
        .lineCoordinateRowsForInteractiveDraft = [this]() {
            return lineCoordinateRowsForInteractiveDraft();
        },
        .areaCoordinateRowsForInteractiveDraft = [this]() {
            return areaCoordinateRowsForInteractiveDraft();
        },
        .captureInteractiveLineAnchor = [this](const QPointF &anchorScenePoint,
                                               const std::optional<QPointF> &dragScenePoint) {
            captureInteractiveLineAnchor(anchorScenePoint, dragScenePoint);
        },
        .hasCompletableInteractiveDrawSession = [this]() {
            return hasCompletableInteractiveDrawSession();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .updateHelpPanel = [this]() {
            updateHelpPanel();
        },
    };
}

void MapEditorTab::setInteractiveDrawMode(InteractiveDrawMode mode)
{
    MapEditorInteractiveDrawController(interactiveDrawContext()).setInteractiveDrawMode(toInteractiveDrawControllerMode(mode));
}

bool MapEditorTab::handleInteractiveDrawClick(const QPointF &scenePosition)
{
    return MapEditorInteractiveDrawController(interactiveDrawContext()).handleInteractiveDrawClick(scenePosition);
}

bool MapEditorTab::commitInteractiveDrawSession()
{
    return MapEditorInteractiveDrawController(interactiveDrawContext()).commitInteractiveDrawSession();
}

void MapEditorTab::clearInteractiveDrawSession(bool clearMode)
{
    MapEditorInteractiveDrawController(interactiveDrawContext()).clearInteractiveDrawSession(clearMode);
}

void MapEditorTab::updateInteractiveDrawPreview()
{
    MapEditorInteractiveDrawController(interactiveDrawContext()).updateInteractiveDrawPreview();
}

bool MapEditorTab::cancelInteractiveDrawingToSelectMode()
{
    return MapEditorInteractiveDrawController(interactiveDrawContext()).cancelInteractiveDrawingToSelectMode();
}
}
