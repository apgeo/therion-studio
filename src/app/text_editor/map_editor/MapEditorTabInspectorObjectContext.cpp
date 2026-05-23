#include "MapEditorTab.h"

#include "MapEditorInspectorObjectContext.h"
#include "../TextEditorTab.h"

namespace TherionStudio
{
MapEditorInspectorObjectContext MapEditorTab::inspectorObjectContext()
{
    return MapEditorInspectorObjectContext{
        .textEditor = textEditor_,
        .objectsTree = mapObjectsTree_,
        .objectsModel = mapObjectsModel_,
        .scene = mapScene_,
        .hiddenObjectLines = &hiddenInspectorObjectLines_,
        .pressedNameIndex = &inspectorObjectPressedNameIndex_,
        .pressedWasSelected = &inspectorObjectPressedWasSelected_,
        .updatingSelection = &updatingMapInspectorObjectSelection_,
        .lastClickedLineNumber = &lastInspectorClickedObjectLineNumber_,
        .suppressedAutoReselectLineNumbers = &suppressedInspectorAutoReselectLineNumbers_,
        .selectedObjectLineNumber = &selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &selectedObjectVertexIndex_,
        .selectedObjectKind = &selectedObjectKind_,
        .selectedObjectCoordinate = &selectedObjectCoordinate_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .filePath = [this]() {
            return filePath();
        },
        .currentLineNumber = [this]() {
            return currentLineNumber();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .updateCommandSurfaceState = [this]() {
            updateCommandSurfaceState();
        },
        .updateGeometrySelectionPresentation = [this]() {
            updateGeometrySelectionPresentation();
        },
        .syncMapSelectionFromTextCursor = [this](int lineNumber, int columnNumber) {
            syncMapSelectionFromTextCursor(lineNumber, columnNumber);
        },
        .selectMapLines = [this](const QSet<int> &lineNumbers, bool centerOnSelection) {
            selectMapLines(lineNumbers, centerOnSelection);
        },
    };
}
}
