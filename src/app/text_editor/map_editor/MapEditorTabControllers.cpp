#include "MapEditorTab.h"

#include <QGraphicsView>
#include <QPoint>
#include <QShortcut>
#include <QTimer>

#include "MapEditorMagnifierOverlay.h"
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
        cancelDrawShortcut_->setEnabled(interactiveDrawState_.mode_ != InteractiveDrawMode::None);
    }
    if (commitDrawShortcut_ != nullptr) {
        commitDrawShortcut_->setEnabled(interactiveDrawState_.mode_ == InteractiveDrawMode::Line
                                        || interactiveDrawState_.mode_ == InteractiveDrawMode::Area);
    }
    refreshBackgroundLayerControls();
    refreshToolbarSummary();
    emit commandSurfaceStateChanged();
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

} // namespace TherionStudio
