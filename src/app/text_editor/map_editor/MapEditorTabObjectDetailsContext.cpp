#include "MapEditorTab.h"

#include "MapEditorObjectDetailsContext.h"
#include "../TextEditorTab.h"

namespace TherionStudio
{
MapEditorObjectDetailsContext MapEditorTab::objectDetailsContext()
{
    return MapEditorObjectDetailsContext{
        .callbackContext = this,
        .textEditor = textEditor_,
        .inspectorSymbolCatalog = &inspectorSymbolCatalog_,
        .orientationApplicabilityByCommand = &orientationApplicabilityByCommand_,
        .updatingUi = &updatingObjectDetailsUi_,
        .commandApplyInProgress = &mapCommandApplyInProgress_,
        .selectedObjectLineNumber = &selectedObjectLineNumber_,
        .selectedObjectVertexIndex = &selectedObjectVertexIndex_,
        .selectedObjectKind = &selectedObjectKind_,
        .objectQuickCommandKind = &objectQuickCommandKind_,
        .hiddenInspectorObjectLines = &hiddenInspectorObjectLines_,
        .lastInspectorClickedObjectLineNumber = &lastInspectorClickedObjectLineNumber_,
        .toolbarStatusNote = &toolbarStatusNote_,
        .selectionLabel = objectDetailsSelectionLabel_,
        .selectionSection = objectSelectionSection_,
        .selectionTitleLabel = objectSelectionTitleLabel_,
        .vertexSection = vertexSelectionSection_,
        .vertexTitleLabel = vertexSelectionTitleLabel_,
        .geometrySection = geometrySelectionSection_,
        .advancedSection = advancedSelectionSection_,
        .deleteButton = objectDeleteButton_,
        .areaReferenceLabel = objectAreaReferenceLabel_,
        .quickFieldsEditor = objectQuickFieldsEditor_,
        .quickIdentifierLabel = objectQuickIdentifierLabel_,
        .quickNameLabel = objectQuickNameLabel_,
        .quickProjectionLabel = objectQuickProjectionLabel_,
        .quickTypeLabel = objectQuickTypeLabel_,
        .quickSubtypeLabel = objectQuickSubtypeLabel_,
        .quickTypeCombo = objectQuickTypeCombo_,
        .quickSubtypeCombo = objectQuickSubtypeCombo_,
        .quickProjectionCombo = objectQuickProjectionCombo_,
        .quickIdentifierEdit = objectQuickIdentifierEdit_,
        .quickNameEdit = objectQuickNameEdit_,
        .vertexActionsEditor = vertexActionsEditor_,
        .vertexInsertBeforeButton = vertexInsertBeforeButton_,
        .vertexInsertAfterButton = vertexInsertAfterButton_,
        .vertexDeleteButton = vertexDeleteButton_,
        .vertexSplitButton = vertexSplitButton_,
        .metadataLabel = objectDetailsMetadataLabel_,
        .orientationEditor = objectOrientationEditor_,
        .linePointPreviousControlCheck = linePointPreviousControlCheck_,
        .linePointSmoothCheck = linePointSmoothCheck_,
        .linePointNextControlCheck = linePointNextControlCheck_,
        .orientationEnabledCheck = objectOrientationEnabledCheck_,
        .orientationSpin = objectOrientationSpin_,
        .linePointLeftSizeEnabledCheck = linePointLeftSizeEnabledCheck_,
        .linePointLeftSizeSpin = linePointLeftSizeSpin_,
        .lineOptionsEditor = lineOptionsEditor_,
        .lineClosedCheck = lineClosedCheck_,
        .lineReversedCheck = lineReversedCheck_,
        .scrapScaleEditor = scrapScaleEditor_,
        .scrapScaleSourceX1Spin = scrapScaleSourceX1Spin_,
        .scrapScaleSourceY1Spin = scrapScaleSourceY1Spin_,
        .scrapScaleSourceX2Spin = scrapScaleSourceX2Spin_,
        .scrapScaleSourceY2Spin = scrapScaleSourceY2Spin_,
        .scrapScaleRealX1Spin = scrapScaleRealX1Spin_,
        .scrapScaleRealY1Spin = scrapScaleRealY1Spin_,
        .scrapScaleRealX2Spin = scrapScaleRealX2Spin_,
        .scrapScaleRealY2Spin = scrapScaleRealY2Spin_,
        .scrapScaleUnitCombo = scrapScaleUnitCombo_,
        .configureButton = objectConfigureButton_,
        .translate = [this](const char *text) {
            return tr(text);
        },
        .mapSourceBoundsForCurrentDocument = [this]() {
            return mapSourceBoundsForCurrentDocument();
        },
        .refreshToolbarSummary = [this]() {
            refreshToolbarSummary();
        },
        .refreshObjectDetailsPanel = [this]() {
            refreshObjectDetailsPanel();
        },
        .clearInspectorObjectSelection = [this]() {
            clearInspectorObjectSelection();
        },
        .selectMapLine = [this](int lineNumber, bool centerOnSelection) {
            selectMapLine(lineNumber, centerOnSelection);
            syncInspectorObjectSelectionToLine(lineNumber);
        },
        .restorePointSelectionLater = [this](int lineNumber) {
            restorePointSelection(lineNumber);
        },
        .restoreLineAnchorSelectionLater = [this](int lineNumber, int sourceVertexIndex) {
            restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
        },
        .rewriteLineOptionToggle = [this](int lineNumber, const QString &optionName, bool enabled, QString *errorMessage) {
            return rewriteLineOptionToggle(lineNumber, optionName, enabled, errorMessage);
        },
        .recordSourceTextSnapshot = [this](const QString &label, const QString &beforeText, const QString &afterText, int insertedLineNumber) {
            recordSourceTextSnapshot(label, beforeText, afterText, insertedLineNumber);
        },
        .insertLineVertexFromSelection = [this](bool before) {
            return insertLineVertexFromSelection(before);
        },
        .splitLineAtSelection = [this]() {
            return splitLineAtSelection();
        },
        .removeLineVertexFromSelection = [this]() {
            return removeLineVertexFromSelection();
        },
        .toggleLineVertexSmoothFromSelection = [this]() {
            return toggleLineVertexSmoothFromSelection();
        },
        .setLineVertexSmoothForSelection = [this](bool smooth) {
            return setLineVertexSmoothForSelection(smooth);
        },
        .setLineVertexControlHandleForSelection = [this](bool incoming, bool enabled) {
            return setLineVertexControlHandleForSelection(incoming, enabled);
        },
    };
}
}
