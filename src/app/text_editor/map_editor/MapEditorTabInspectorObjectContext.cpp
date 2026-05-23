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
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .selectedObjectLineNumber = &selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &selectedObjectVertexIndex_,
        .selectedObjectKind = &selectedObjectKind_,
        .selectedObjectCoordinate = &selectedObjectCoordinate_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .filePath = [this]() {
            return filePath();
        },
        .currentLineNumber = [this]() {
            return currentLineNumber();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
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
        .recordSourceTextSnapshot = [this](const QString &label, const QString &beforeText, const QString &afterText, int insertedLineNumber) {
            recordSourceTextSnapshot(label, beforeText, afterText, insertedLineNumber);
        },
    };
}
}
