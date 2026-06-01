#include "MapEditorTab.h"

#include <QModelIndex>

#include "MapEditorInspectorBackgroundController.h"
#include "MapEditorInspectorObjectController.h"
#include "MapEditorObjectDetailsEditController.h"
#include "MapEditorObjectDetailsPanelController.h"

namespace TherionStudio
{

void MapEditorTab::updateHelpPanel()
{
    // Map-specific help was removed; source contextual help is owned by the embedded TextEditorTab.
}

void MapEditorTab::rebuildInspectorObjectsTree()
{
    MapEditorInspectorObjectController(inspectorObjectContext()).rebuildInspectorObjectsTree();
}

void MapEditorTab::configureInspectorObjectTreeColumns()
{
    MapEditorInspectorObjectController(inspectorObjectContext()).configureInspectorObjectTreeColumns();
}

QModelIndex MapEditorTab::findInspectorObjectIndexForLine(int lineNumber) const
{
    return MapEditorInspectorObjectController(const_cast<MapEditorTab *>(this)->inspectorObjectContext()).findInspectorObjectIndexForLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).syncInspectorObjectSelectionToLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).syncInspectorObjectSelectionToLine(lineNumber, scrollToSelection);
}

void MapEditorTab::setInspectorObjectCurrentIndex(const QModelIndex &index)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).setInspectorObjectCurrentIndex(index);
}

void MapEditorTab::clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).clearInspectorObjectSelection(suppressAutoReselectLineNumbers);
}

void MapEditorTab::handleInspectorObjectSelectionChanged(const QModelIndex &current)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).handleInspectorObjectSelectionChanged(current);
}

void MapEditorTab::handleInspectorObjectClicked(const QModelIndex &index)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).handleInspectorObjectClicked(index);
}

void MapEditorTab::applyInspectorObjectVisibility()
{
    MapEditorInspectorObjectController(inspectorObjectContext()).applyInspectorObjectVisibility();
}

void MapEditorTab::configureInspectorBackgroundLayerTreeColumns()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).configureInspectorBackgroundLayerTreeColumns();
}

void MapEditorTab::handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current)
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).handleInspectorBackgroundLayerSelectionChanged(current);
}

void MapEditorTab::handleInspectorBackgroundLayerClicked(const QModelIndex &index)
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).handleInspectorBackgroundLayerClicked(index);
}

void MapEditorTab::refreshInspectorBackgroundPanel()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).refreshInspectorBackgroundPanel();
}

void MapEditorTab::refreshInspectorBackgroundSelectionControls()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).refreshInspectorBackgroundSelectionControls();
}

void MapEditorTab::refreshObjectDetailsPanel()
{
    MapEditorObjectDetailsPanelController(objectDetailsContext()).refreshObjectDetailsPanel();
}

void MapEditorTab::handleObjectOrientationValueChanged(double value)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleObjectOrientationValueChanged(value);
}

void MapEditorTab::handleObjectOrientationEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleObjectOrientationEnabledToggled(checked);
}

void MapEditorTab::handleLinePointLeftSizeValueChanged(double value)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointLeftSizeValueChanged(value);
}

void MapEditorTab::handleLinePointLeftSizeEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointLeftSizeEnabledToggled(checked);
}

void MapEditorTab::deleteSelectedObjectFromSelection()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).deleteSelectedObjectFromSelection();
}

void MapEditorTab::applyObjectQuickFieldEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectQuickFieldEdits();
}

void MapEditorTab::applyScrapProjectionEdit()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyScrapProjectionEdit();
}

void MapEditorTab::updateObjectQuickSubtypeChoices()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).updateObjectQuickSubtypeChoices();
}

void MapEditorTab::insertVertexBeforeFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).insertVertexBeforeFromSelectionPanel();
}

void MapEditorTab::insertVertexAfterFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).insertVertexAfterFromSelectionPanel();
}

void MapEditorTab::splitLineFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).splitLineFromSelectionPanel();
}

void MapEditorTab::deleteVertexFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).deleteVertexFromSelectionPanel();
}

void MapEditorTab::handleLinePointPreviousControlToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointPreviousControlToggled(checked);
}

void MapEditorTab::handleLinePointSmoothToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointSmoothToggled(checked);
}

void MapEditorTab::handleLinePointNextControlToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointNextControlToggled(checked);
}

void MapEditorTab::applyLinePointFlagsEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyLinePointFlagsEdits();
    objectDetailsUiState_.linePointFlagsDirty_ = false;
}

void MapEditorTab::populateScrapScaleFromSourceBounds()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).populateScrapScaleFromSourceBounds();
}

void MapEditorTab::applyScrapScaleEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyScrapScaleEdits();
}

void MapEditorTab::handleConfigureObjectSettingsTriggered()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleConfigureObjectSettingsTriggered();
}

void MapEditorTab::handleLineClosedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLineClosedToggled(checked);
}

void MapEditorTab::handleLineReversedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLineReversedToggled(checked);
}

} // namespace TherionStudio
