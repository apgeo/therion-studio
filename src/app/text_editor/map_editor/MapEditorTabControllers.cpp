#include "MapEditorTab.h"

#include <QGraphicsView>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QModelIndex>
#include <QPoint>
#include <QShortcut>
#include <QStandardItemModel>
#include <QTimer>

#include "MapEditorInspectorBackgroundController.h"
#include "MapEditorInspectorObjectController.h"
#include "MapEditorMagnifierOverlay.h"
#include "MapEditorObjectDetailsEditController.h"
#include "MapEditorObjectDetailsPanelController.h"
#include "MapEditorSceneLifecycleController.h"

namespace TherionStudio
{
namespace
{
constexpr int kMagnifierUpdateIntervalMs = 16;
}

void MapEditorTab::updateCommandSurfaceState()
{
    if (cancelDrawShortcut_ != nullptr) {
        cancelDrawShortcut_->setEnabled(interactiveDrawMode_ != InteractiveDrawMode::None);
    }
    if (commitDrawShortcut_ != nullptr) {
        commitDrawShortcut_->setEnabled(interactiveDrawMode_ == InteractiveDrawMode::Line
                                        || interactiveDrawMode_ == InteractiveDrawMode::Area);
    }
    refreshBackgroundLayerControls();
    refreshToolbarSummary();
    emit commandSurfaceStateChanged();
}

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

void MapEditorTab::clearMapScene()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).clearMapScene();
}

void MapEditorTab::clearDraftGeometryItems()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).clearDraftGeometryItems();
}

void MapEditorTab::clearBackgroundImageItems()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).clearBackgroundImageItems();
}

void MapEditorTab::restoreDraftGeometryItems()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).restoreDraftGeometryItems();
}

void MapEditorTab::restoreBackgroundImageItems()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).restoreBackgroundImageItems();
}

void MapEditorTab::fitMapToView(bool includeBackgroundImages)
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).fitMapToView(includeBackgroundImages);
}

void MapEditorTab::syncZoomFactorFromView()
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).syncZoomFactorFromView();
}

void MapEditorTab::applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition)
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).applyZoomAtViewportPosition(factor, viewportPosition);
    if (mapMagnifierOverlay_ != nullptr && mapMagnifierOverlay_->isVisible()) {
        scheduleMagnifierOverlayUpdateFromViewportPosition(viewportPosition.toPoint());
    }
}

void MapEditorTab::refreshToolbarSummary()
{
    // Status text is retained for future command-surface summaries.
}

void MapEditorTab::updateMagnifierOverlayGeometry()
{
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->updatePlacement();
    }
}

void MapEditorTab::updateMagnifierOverlayFromViewportPosition(const QPoint &viewportPosition, bool active)
{
    if (mapMagnifierOverlay_ == nullptr || mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }
    if (!mapView_->viewport()->rect().contains(viewportPosition)) {
        hideMagnifierOverlay();
        return;
    }

    const QPointF scenePosition = mapView_->mapToScene(viewportPosition);
    mapMagnifierOverlay_->setCursorPosition(viewportPosition,
                                            scenePosition,
                                            magnifierCoordinateTextForScenePosition(scenePosition));
    mapMagnifierOverlay_->setOverlayActive(active);
}

void MapEditorTab::scheduleMagnifierOverlayUpdateFromViewportPosition(const QPoint &viewportPosition)
{
    if (mapMagnifierOverlay_ == nullptr || mapView_ == nullptr || mapView_->viewport() == nullptr) {
        return;
    }

    magnifierPendingViewportPosition_ = viewportPosition;
    magnifierLastViewportPosition_ = viewportPosition;
    magnifierHasViewportPosition_ = true;
    if (magnifierUpdatePending_) {
        return;
    }

    const int elapsedMs = magnifierThrottleActive_
        ? static_cast<int>(magnifierLastUpdateElapsed_.elapsed())
        : kMagnifierUpdateIntervalMs;
    const int delayMs = qMax(0, kMagnifierUpdateIntervalMs - elapsedMs);

    magnifierUpdatePending_ = true;
    QTimer::singleShot(delayMs, this, [this]() {
        magnifierUpdatePending_ = false;
        magnifierThrottleActive_ = true;
        magnifierLastUpdateElapsed_.restart();
        updateMagnifierOverlayFromViewportPosition(magnifierPendingViewportPosition_, true);
    });
}

void MapEditorTab::refreshVisibleMagnifierOverlay()
{
    if (mapMagnifierOverlay_ == nullptr || !mapMagnifierOverlay_->isVisible() || !magnifierHasViewportPosition_) {
        return;
    }

    scheduleMagnifierOverlayUpdateFromViewportPosition(magnifierLastViewportPosition_);
}

void MapEditorTab::hideMagnifierOverlay()
{
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->setOverlayActive(false);
    }
    magnifierHasViewportPosition_ = false;
    magnifierThrottleActive_ = false;
}

QString MapEditorTab::magnifierCoordinateTextForScenePosition(const QPointF &scenePosition) const
{
    const QPointF sourcePosition = sourcePointFromScenePosition(scenePosition);
    return tr("x %1  y %2").arg(QString::number(sourcePosition.x(), 'f', 1),
                                QString::number(sourcePosition.y(), 'f', 1));
}

QRectF MapEditorTab::mapGeometryFitBounds() const
{
    return MapEditorSceneLifecycleController(sceneLifecycleContext()).mapGeometryFitBounds();
}

QRectF MapEditorTab::mapPreviewBounds() const
{
    return MapEditorSceneLifecycleController(sceneLifecycleContext()).mapPreviewBounds();
}

void MapEditorTab::adjustMapZoom(qreal factor)
{
    MapEditorSceneLifecycleController(sceneLifecycleContext()).adjustMapZoom(factor);
}

}
