#include "MapEditorSceneLifecycleController.h"

#include "MapEditorSceneSupport.h"
#include "MapEditorTab.h"

#include <QGraphicsItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScopedValueRollback>
#include <QTransform>

#include <cmath>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kMapItemRole = Qt::UserRole + 120;
constexpr int kMapItemGeometryValue = 1;
}

MapEditorSceneLifecycleController::MapEditorSceneLifecycleController(MapEditorTab *owner)
    : owner_(owner)
{
}

MapEditorSceneLifecycleController::MapEditorSceneLifecycleController(const MapEditorTab *owner)
    : owner_(const_cast<MapEditorTab *>(owner))
{
}

void MapEditorSceneLifecycleController::clearMapScene()
{
    owner_->mapItemsByLine_.clear();
    owner_->interactiveDrawStrokeActive_ = false;
    owner_->interactiveDrawPreviewPath_ = nullptr;
    owner_->interactiveDrawPreviewMarkers_.clear();
    if (owner_->mapScene_ == nullptr) {
        return;
    }
    QScopedValueRollback<bool> selectionGuard(owner_->updatingSelection_, true);

    QVector<QGraphicsRectItem *> preservedDrafts;
    QVector<QGraphicsPixmapItem *> preservedBackgrounds;
    const QList<QGraphicsItem *> items = owner_->mapScene_->items();
    for (QGraphicsItem *item : items) {
        if (auto *backgroundItem = dynamic_cast<QGraphicsPixmapItem *>(item)) {
            owner_->mapScene_->removeItem(backgroundItem);
            preservedBackgrounds.append(backgroundItem);
            continue;
        }
        if (auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item)) {
            owner_->mapScene_->removeItem(draftItem);
            preservedDrafts.append(draftItem);
            continue;
        }

        owner_->mapScene_->removeItem(item);
        delete item;
    }

    owner_->draftGeometryItems_ = preservedDrafts;
    owner_->backgroundImageItems_ = preservedBackgrounds;
}

void MapEditorSceneLifecycleController::clearDraftGeometryItems()
{
    if (owner_->mapScene_ != nullptr) {
        for (QGraphicsRectItem *item : std::as_const(owner_->draftGeometryItems_)) {
            if (item != nullptr) {
                owner_->mapScene_->removeItem(item);
                delete item;
            }
        }
    }

    owner_->draftGeometryItems_.clear();
    owner_->nextDraftGeometryId_ = 1;
}

void MapEditorSceneLifecycleController::clearBackgroundImageItems()
{
    if (owner_->mapScene_ != nullptr) {
        for (QGraphicsPixmapItem *item : std::as_const(owner_->backgroundImageItems_)) {
            if (item != nullptr) {
                owner_->mapScene_->removeItem(item);
                delete item;
            }
        }
    }

    owner_->backgroundImageItems_.clear();
    owner_->selectedBackgroundLayerIndex_ = -1;
    owner_->refreshBackgroundLayerControls();
}

void MapEditorSceneLifecycleController::restoreDraftGeometryItems()
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    for (QGraphicsRectItem *item : std::as_const(owner_->draftGeometryItems_)) {
        if (item == nullptr || owner_->mapScene_->items().contains(item)) {
            continue;
        }

        owner_->mapScene_->addItem(item);
        item->setZValue(20.0);
    }
}

void MapEditorSceneLifecycleController::restoreBackgroundImageItems()
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    for (QGraphicsPixmapItem *item : std::as_const(owner_->backgroundImageItems_)) {
        if (item == nullptr || owner_->mapScene_->items().contains(item)) {
            continue;
        }

        owner_->mapScene_->addItem(item);
    }

    owner_->applyBackgroundLayerStackingOrder();
    owner_->setSelectedBackgroundLayerIndexInternal(owner_->selectedBackgroundLayerIndex_);
    owner_->refreshBackgroundLayerControls();
}

void MapEditorSceneLifecycleController::fitMapToView(bool includeBackgroundImages)
{
    if (owner_->mapScene_ == nullptr || owner_->mapView_ == nullptr) {
        return;
    }

    QRectF fitBounds = mapGeometryFitBounds();
    if (includeBackgroundImages) {
        const QRectF backgroundBounds = owner_->mapBackgroundFitBounds();
        if (backgroundBounds.isValid()) {
            fitBounds = fitBounds.isValid() ? fitBounds.united(backgroundBounds) : backgroundBounds;
        }
    }
    if (!fitBounds.isValid()) {
        if (owner_->mapScene_->items().isEmpty()) {
            return;
        }

        fitBounds = owner_->mapScene_->itemsBoundingRect();
    }

    owner_->mapView_->resetTransform();
    owner_->mapView_->fitInView(fitBounds.adjusted(-24, -24, 24, 24), Qt::KeepAspectRatio);
    owner_->autoFitEnabled_ = true;
    syncZoomFactorFromView();
    owner_->updateCommandSurfaceState();
}

void MapEditorSceneLifecycleController::syncZoomFactorFromView()
{
    if (owner_->mapView_ == nullptr) {
        owner_->zoomFactor_ = 1.0;
        emit owner_->zoomStatusChanged(owner_->zoomPercent());
        return;
    }

    const QTransform viewTransform = owner_->mapView_->transform();
    const qreal scaleX = std::hypot(viewTransform.m11(), viewTransform.m21());
    owner_->zoomFactor_ = scaleX > 0.0 ? scaleX : 1.0;
    emit owner_->zoomStatusChanged(owner_->zoomPercent());
}

void MapEditorSceneLifecycleController::applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition)
{
    if (owner_->mapView_ == nullptr || factor <= 0.0) {
        return;
    }

    syncZoomFactorFromView();
    const qreal targetZoomFactor = qBound(0.1, owner_->zoomFactor_ * factor, 50.0);
    if (qFuzzyCompare(targetZoomFactor, owner_->zoomFactor_)) {
        return;
    }

    const qreal appliedFactor = targetZoomFactor / owner_->zoomFactor_;
    const QPoint viewportPoint = viewportPosition.toPoint();
    const QPointF scenePointBefore = owner_->mapView_->mapToScene(viewportPoint);
    owner_->mapView_->scale(appliedFactor, appliedFactor);
    const QPointF scenePointAfter = owner_->mapView_->mapToScene(viewportPoint);
    const QPointF sceneDelta = scenePointAfter - scenePointBefore;
    owner_->mapView_->translate(sceneDelta.x(), sceneDelta.y());

    owner_->autoFitEnabled_ = false;
    syncZoomFactorFromView();
    owner_->updateCommandSurfaceState();
}

QRectF MapEditorSceneLifecycleController::mapGeometryFitBounds() const
{
    if (owner_->mapScene_ == nullptr) {
        return QRectF();
    }

    QRectF bounds;
    bool hasBounds = false;
    const QList<QGraphicsItem *> items = owner_->mapScene_->items();
    for (QGraphicsItem *item : items) {
        if (item == nullptr) {
            continue;
        }
        if (item->data(kMapItemRole).toInt() != kMapItemGeometryValue) {
            continue;
        }

        const QRectF itemBounds = item->sceneBoundingRect();
        if (!itemBounds.isValid()) {
            continue;
        }

        if (!hasBounds) {
            bounds = itemBounds;
            hasBounds = true;
        } else {
            bounds = bounds.united(itemBounds);
        }
    }

    return hasBounds ? bounds : QRectF();
}

QRectF MapEditorSceneLifecycleController::mapPreviewBounds() const
{
    if (owner_->mapScene_ == nullptr) {
        return QRectF();
    }

    const QRectF sceneFrame = owner_->mapScene_->sceneRect();
    if (sceneFrame.width() <= 80.0 || sceneFrame.height() <= 80.0) {
        return QRectF();
    }

    const QRectF geometryCanvas = sceneFrame.adjusted(24.0, 24.0, -24.0, -24.0);
    return geometryCanvas.adjusted(20.0, 20.0, -20.0, -20.0);
}

void MapEditorSceneLifecycleController::adjustMapZoom(qreal factor)
{
    if (owner_->mapView_ == nullptr) {
        return;
    }

    applyZoomAtViewportPosition(factor, owner_->mapView_->viewport()->rect().center());
}

}
