#include "MapEditorTab.h"

#include "MapEditorSelectionContext.h"
#include "MapEditorSelectionController.h"

namespace TherionStudio
{
MapEditorSelectionContext MapEditorTab::selectionContext()
{
    return MapEditorSelectionContext{
        .textEditor = textEditor_,
        .scene = mapScene_,
        .view = mapView_,
        .itemsByLine = &mapItemsByLine_,
        .hiddenObjectLines = &hiddenInspectorObjectLines_,
        .suppressedAutoReselectLineNumbers = &suppressedInspectorAutoReselectLineNumbers_,
        .updatingSelection = &updatingSelection_,
        .autoFitEnabled = &autoFitEnabled_,
        .textNavigationInProgress = &mapSelectionDrivenTextNavigationInProgress_,
        .lastCursorSyncedLine = &lastCursorSyncedLine_,
        .lastCursorSyncedColumn = &lastCursorSyncedColumn_,
        .pendingClickSelection = &pendingMapClickSelection_,
        .pendingClickScenePosition = &pendingMapClickScenePosition_,
        .pendingClickElapsed = &pendingMapClickElapsed_,
        .pendingClickLineNumber = &pendingMapClickLineNumber_,
        .pendingClickSourceVertexIndex = &pendingMapClickSourceVertexIndex_,
        .pendingClickGeometryKind = &pendingMapClickGeometryKind_,
        .selectedObjectLineNumber = &selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &selectedObjectVertexIndex_,
        .selectedObjectKind = &selectedObjectKind_,
        .selectedObjectCoordinate = &selectedObjectCoordinate_,
        .currentLineNumber = [this]() {
            return currentLineNumber();
        },
        .sourcePointFromScenePosition = [this](const QPointF &scenePosition) {
            return sourcePointFromScenePosition(scenePosition);
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .updateHelpPanel = [this]() {
            updateHelpPanel();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
    };
}

void MapEditorTab::handleMapSceneSelectionChanged()
{
    MapEditorSelectionController(selectionContext()).handleMapSceneSelectionChanged();
}

void MapEditorTab::syncMapSelectionFromTextCursor(int lineNumber, int columnNumber)
{
    MapEditorSelectionController(selectionContext()).syncMapSelectionFromTextCursor(lineNumber, columnNumber);
}

void MapEditorTab::updateGeometrySelectionPresentation()
{
    MapEditorSelectionController(selectionContext()).updateGeometrySelectionPresentation();
}

void MapEditorTab::selectMapLine(int lineNumber, bool centerOnSelection)
{
    MapEditorSelectionController(selectionContext()).selectMapLine(lineNumber, centerOnSelection);
}

void MapEditorTab::selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection)
{
    MapEditorSelectionController(selectionContext()).selectMapLines(lineNumbers, centerOnSelection);
}
}
