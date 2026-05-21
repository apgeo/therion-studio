#include "MapEditorViewportInputController.h"

#include "../TextEditorTab.h"
#include "MapEditorInputPolicy.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorTab.h"

#include <QDateTime>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace TherionStudio
{
namespace
{
bool wheelEventHasPreciseScrollingDeltas(const QWheelEvent *event)
{
    if (event == nullptr) {
        return false;
    }

    if (!event->pixelDelta().isNull()) {
        return true;
    }

    return event->phase() != Qt::NoScrollPhase;
}

bool isInteractiveMapSelectionItem(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return false;
    }
    if (!item->isVisible()) {
        return false;
    }
    if (!(item->flags() & QGraphicsItem::ItemIsSelectable)) {
        return false;
    }
    if (item->acceptedMouseButtons() == Qt::NoButton) {
        return false;
    }
    return true;
}

int mapSelectionHitPriority(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return 100;
    }

    if (dynamic_cast<const MapEditablePointItem *>(item) != nullptr) {
        return 0;
    }
    if (auto *vertexItem = dynamic_cast<const MapEditableGeometryVertexItem *>(item)) {
        const QString geometryKind = vertexItem->geometryKind().trimmed().toLower();
        if (geometryKind == QStringLiteral("line") || geometryKind == QStringLiteral("area")) {
            return 0;
        }
        if (geometryKind.startsWith(QStringLiteral("line control"))) {
            return 2;
        }
    }

    const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
    switch (subtype) {
    case kMapSceneSelectionSubtypeLineAnchor:
    case kMapSceneSelectionSubtypeAreaVertex:
        return 0;
    case kMapSceneSelectionSubtypeLineControl:
        return 2;
    case kMapSceneSelectionSubtypePointOrientationHandle:
        return 2;
    case kMapSceneSelectionSubtypeLineControlConnector:
        return 3;
    case kMapSceneSelectionSubtypeLineDetail:
        return 4;
    default:
        break;
    }

    return 1;
}

QGraphicsItem *preferredMapHitItem(const QList<QGraphicsItem *> &hitItems, bool requireSelected = false)
{
    QGraphicsItem *bestItem = nullptr;
    int bestPriority = std::numeric_limits<int>::max();
    for (QGraphicsItem *item : hitItems) {
        if (!isInteractiveMapSelectionItem(item)) {
            continue;
        }
        if (requireSelected && !item->isSelected()) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber <= 0) {
            continue;
        }

        const int priority = mapSelectionHitPriority(item);
        if (priority < bestPriority) {
            bestPriority = priority;
            bestItem = item;
            if (bestPriority == 0) {
                break;
            }
        }
    }
    return bestItem;
}

}

MapEditorViewportInputController::MapEditorViewportInputController(MapEditorTab *owner)
    : owner_(owner)
{
}

std::optional<bool> MapEditorViewportInputController::handleEvent(QObject *watched, QEvent *event)
{
    if (owner_->mapView_ == nullptr) {
        return std::nullopt;
    }

    QWidget *viewport = owner_->mapView_->viewport();
    if (watched == viewport) {
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
            owner_->lastTabletInteractionUtc_ = QDateTime::currentDateTimeUtc();
            break;
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Freehand) {
                    if (owner_->textEditor_ == nullptr) {
                        owner_->toolbarStatusNote_ = owner_->tr("Drawing failed: no active TH2 text editor.");
                        owner_->refreshToolbarSummary();
                        event->accept();
                        return true;
                    }

                    owner_->clearInteractiveDrawSession(false);
                    const QPointF scenePoint = owner_->mapView_->mapToScene(mouseEvent->pos());
                    owner_->interactiveDrawStrokeActive_ = true;
                    owner_->interactiveDrawSourceVertices_.append(owner_->sourcePointFromScenePosition(scenePoint));
                    owner_->interactiveDrawSceneVertices_.append(scenePoint);
                    owner_->updateInteractiveDrawPreview();
                    owner_->toolbarStatusNote_ = owner_->tr("Freehand mode: drawing stroke...");
                    owner_->refreshToolbarSummary();
                    owner_->updateCommandSurfaceState();
                    owner_->primaryPointerInteractionActive_ = false;
                    event->accept();
                    return true;
                }
                if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                    || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area) {
                    const QPointF scenePoint = owner_->mapView_->mapToScene(mouseEvent->pos());
                    const QPointF sceneOffset = owner_->mapView_->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (const auto handle = owner_->interactiveLineControlAt(scenePoint, controlHitRadius)) {
                        owner_->interactiveDrawControlDragActive_ = true;
                        owner_->interactiveDrawControlDragHandle_ = handle.value();
                        owner_->interactiveDrawAnchorPressActive_ = false;
                        owner_->interactiveDrawAnchorDragActive_ = false;
                        owner_->interactiveDrawHoverActive_ = false;
                        viewport->setCursor(Qt::ClosedHandCursor);
                        owner_->toolbarStatusNote_ = owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                            ? owner_->tr("Line mode: dragging bezier control point.")
                            : owner_->tr("Area mode: dragging bezier control point.");
                        owner_->refreshToolbarSummary();
                        owner_->updateCommandSurfaceState();
                        owner_->primaryPointerInteractionActive_ = false;
                        event->accept();
                        return true;
                    }

                    owner_->interactiveDrawAnchorPressActive_ = true;
                    owner_->interactiveDrawAnchorPressScenePoint_ = scenePoint;
                    owner_->interactiveDrawAnchorDragActive_ = false;
                    owner_->interactiveDrawAnchorDragScenePoint_ = owner_->interactiveDrawAnchorPressScenePoint_;
                    owner_->interactiveDrawControlDragActive_ = false;
                    owner_->interactiveDrawHoverActive_ = false;
                    owner_->updateInteractiveDrawPreview();
                    owner_->primaryPointerInteractionActive_ = false;
                    event->accept();
                    return true;
                }
                if (owner_->handleInteractiveDrawClick(owner_->mapView_->mapToScene(mouseEvent->pos()))) {
                    owner_->primaryPointerInteractionActive_ = false;
                    event->accept();
                    return true;
                }
                owner_->primaryPointerInteractionActive_ = true;
                if (owner_->mapView_ != nullptr) {
                    owner_->pendingMapClickSelection_ = true;
                    owner_->pendingMapClickScenePosition_ = owner_->mapView_->mapToScene(mouseEvent->pos());
                    owner_->pendingMapClickElapsed_.start();
                    owner_->pendingMapClickLineNumber_ = 0;
                    owner_->pendingMapClickSourceVertexIndex_ = -1;
                    owner_->pendingMapClickGeometryKind_.clear();
                    if (owner_->mapScene_ != nullptr) {
                        const QList<QGraphicsItem *> hitItems = owner_->mapScene_->items(owner_->pendingMapClickScenePosition_,
                                                                                 Qt::IntersectsItemShape,
                                                                                 Qt::DescendingOrder,
                                                                                 owner_->mapView_->transform());
                        if (QGraphicsItem *item = preferredMapHitItem(hitItems)) {
                            const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
                            owner_->pendingMapClickLineNumber_ = lineNumber;
                            if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
                                owner_->pendingMapClickSourceVertexIndex_ = vertexItem->vertexIndex();
                                owner_->pendingMapClickGeometryKind_ = vertexItem->geometryKind();
                            } else if (item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineControlConnector) {
                                const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
                                if (ownerVertexIndex >= 0) {
                                    owner_->pendingMapClickSourceVertexIndex_ = ownerVertexIndex;
                                    owner_->pendingMapClickGeometryKind_ = QStringLiteral("line");
                                }
                            }
                        }
                    }
                }
            }

            if (mouseEvent->button() == Qt::RightButton) {
                owner_->mapPanActive_ = true;
                owner_->mapPanLastPosition_ = mouseEvent->pos();
                viewport->setCursor(Qt::ClosedHandCursor);
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if ((owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                 || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area)
                && owner_->interactiveDrawControlDragActive_) {
                const QPointF scenePoint = owner_->mapView_->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                if (owner_->setInteractiveLineControlScenePoint(owner_->interactiveDrawControlDragHandle_, scenePoint)) {
                    owner_->updateInteractiveDrawPreview();
                }
                event->accept();
                return true;
            }

            if ((owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                 || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area)
                && owner_->interactiveDrawAnchorPressActive_) {
                const QPointF scenePoint = owner_->mapView_->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal dragThreshold = 4.0;
                if (!owner_->interactiveDrawAnchorDragActive_
                    && QLineF(owner_->interactiveDrawAnchorPressScenePoint_, scenePoint).length() >= dragThreshold) {
                    owner_->interactiveDrawAnchorDragActive_ = true;
                }
                owner_->interactiveDrawAnchorDragScenePoint_ = scenePoint;
                owner_->updateInteractiveDrawPreview();
                event->accept();
                return true;
            }

            if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Freehand && owner_->interactiveDrawStrokeActive_) {
                const QPointF scenePoint = owner_->mapView_->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal minimumSceneSampleDistance = 4.0;
                if (owner_->interactiveDrawSceneVertices_.isEmpty()
                    || QLineF(owner_->interactiveDrawSceneVertices_.last(), scenePoint).length() >= minimumSceneSampleDistance) {
                    owner_->interactiveDrawSceneVertices_.append(scenePoint);
                    owner_->interactiveDrawSourceVertices_.append(owner_->sourcePointFromScenePosition(scenePoint));
                    owner_->updateInteractiveDrawPreview();
                    owner_->updateCommandSurfaceState();
                }
                event->accept();
                return true;
            }

            if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area) {
                const bool hasDraftVertices = !owner_->interactiveDrawLineVertices_.isEmpty();
                if (hasDraftVertices) {
                    const QPoint mousePosition = static_cast<QMouseEvent *>(event)->pos();
                    const QPointF scenePoint = owner_->mapView_->mapToScene(mousePosition);
                    const QPointF sceneOffset = owner_->mapView_->mapToScene(mousePosition + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (owner_->interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                        viewport->setCursor(Qt::OpenHandCursor);
                    } else {
                        viewport->unsetCursor();
                    }
                    owner_->interactiveDrawHoverActive_ = true;
                    owner_->interactiveDrawHoverScenePoint_ = scenePoint;
                    owner_->updateInteractiveDrawPreview();
                    event->accept();
                    return true;
                }
            }

            if (!owner_->mapPanActive_) {
                break;
            }

            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint delta = mouseEvent->pos() - owner_->mapPanLastPosition_;
            owner_->mapPanLastPosition_ = mouseEvent->pos();
            if (owner_->mapView_->horizontalScrollBar() != nullptr) {
                owner_->mapView_->horizontalScrollBar()->setValue(owner_->mapView_->horizontalScrollBar()->value() - delta.x());
            }
            if (owner_->mapView_->verticalScrollBar() != nullptr) {
                owner_->mapView_->verticalScrollBar()->setValue(owner_->mapView_->verticalScrollBar()->value() - delta.y());
            }

            owner_->autoFitEnabled_ = false;
            owner_->syncZoomFactorFromView();
            owner_->updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                 || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area)
                && owner_->interactiveDrawControlDragActive_
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF scenePoint = owner_->mapView_->mapToScene(mouseEvent->pos());
                owner_->setInteractiveLineControlScenePoint(owner_->interactiveDrawControlDragHandle_, scenePoint);
                owner_->interactiveDrawControlDragActive_ = false;
                const QPointF sceneOffset = owner_->mapView_->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                if (owner_->interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                    viewport->setCursor(Qt::OpenHandCursor);
                } else {
                    viewport->unsetCursor();
                }
                owner_->toolbarStatusNote_ = owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                    ? owner_->tr("Line mode: bezier control adjusted.")
                    : owner_->tr("Area mode: bezier control adjusted.");
                owner_->refreshToolbarSummary();
                owner_->updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if ((owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                 || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area)
                && owner_->interactiveDrawAnchorPressActive_
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF anchorScenePoint = owner_->interactiveDrawAnchorPressScenePoint_;
                std::optional<QPointF> dragScenePoint;
                if (owner_->interactiveDrawAnchorDragActive_) {
                    const QPointF releaseScenePoint = owner_->mapView_->mapToScene(mouseEvent->pos());
                    constexpr qreal dragThreshold = 4.0;
                    if (QLineF(owner_->interactiveDrawAnchorPressScenePoint_, releaseScenePoint).length() >= dragThreshold) {
                        dragScenePoint = releaseScenePoint;
                    }
                }

                owner_->interactiveDrawAnchorPressActive_ = false;
                owner_->interactiveDrawAnchorDragActive_ = false;
                owner_->interactiveDrawHoverActive_ = false;
                owner_->captureInteractiveLineAnchor(anchorScenePoint, dragScenePoint);
                owner_->toolbarStatusNote_ = owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                    ? owner_->tr("Line mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg(owner_->interactiveDrawLineVertices_.size())
                    : owner_->tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg(owner_->interactiveDrawLineVertices_.size());
                owner_->refreshToolbarSummary();
                owner_->updateCommandSurfaceState();
                event->accept();
                return true;
            }
            if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Freehand
                && owner_->interactiveDrawStrokeActive_
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF releasePoint = owner_->mapView_->mapToScene(mouseEvent->pos());
                if (owner_->interactiveDrawSceneVertices_.isEmpty()
                    || QLineF(owner_->interactiveDrawSceneVertices_.last(), releasePoint).length() >= 1.0) {
                    owner_->interactiveDrawSceneVertices_.append(releasePoint);
                    owner_->interactiveDrawSourceVertices_.append(owner_->sourcePointFromScenePosition(releasePoint));
                }
                owner_->interactiveDrawStrokeActive_ = false;

                if (owner_->interactiveDrawSourceVertices_.size() < 2) {
                    owner_->clearInteractiveDrawSession(false);
                    owner_->toolbarStatusNote_ = owner_->tr("Freehand mode needs a drag stroke to create a line.");
                    owner_->refreshToolbarSummary();
                    owner_->updateCommandSurfaceState();
                    event->accept();
                    return true;
                }

                const bool committed = owner_->commitInteractiveDrawVertices(QStringLiteral("line"),
                                                                     owner_->interactiveDrawSourceVertices_,
                                                                     owner_->tr("freehand line"));
                owner_->clearInteractiveDrawSession(false);
                if (committed) {
                    owner_->updateHelpPanel();
                }
                owner_->refreshToolbarSummary();
                owner_->updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if (mouseEvent->button() == Qt::LeftButton && mouseEvent->buttons() == Qt::NoButton) {
                owner_->primaryPointerInteractionActive_ = false;
            }

            if (owner_->mapPanActive_ && mouseEvent->button() == Qt::RightButton) {
                owner_->mapPanActive_ = false;
                viewport->unsetCursor();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::Wheel: {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            if (owner_->nativeZoomGestureActive_
                && owner_->lastNativeZoomGestureUtc_.isValid()
                && owner_->lastNativeZoomGestureUtc_.msecsTo(QDateTime::currentDateTimeUtc()) > 1500) {
                owner_->nativeZoomGestureActive_ = false;
            }

            const bool recentNativeZoom = owner_->lastNativeZoomGestureUtc_.isValid()
                && owner_->lastNativeZoomGestureUtc_.msecsTo(QDateTime::currentDateTimeUtc()) <= 150;
            if (owner_->nativeZoomGestureActive_ || recentNativeZoom) {
                event->accept();
                return true;
            }

            if (owner_->primaryPointerInteractionActive_) {
                event->accept();
                return true;
            }

            const Qt::KeyboardModifiers modifiers = wheelEvent->modifiers();
            const bool cmdModifier = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier);
            const bool preciseScroll = wheelEventHasPreciseScrollingDeltas(wheelEvent);
            const MapEditorWheelAction wheelAction = resolveMapEditorWheelAction(owner_->touchFriendlyControlsEnabled_,
                                                                                 preciseScroll,
                                                                                 cmdModifier);
            if (wheelAction == MapEditorWheelAction::Zoom) {
                const QPoint pixelDelta = wheelEvent->pixelDelta();
                const QPoint angleDelta = wheelEvent->angleDelta();
                qreal delta = !pixelDelta.isNull()
                    ? static_cast<qreal>(pixelDelta.y())
                    : static_cast<qreal>(angleDelta.y());
                if (qFuzzyIsNull(delta) && !angleDelta.isNull()) {
                    delta = static_cast<qreal>(angleDelta.x());
                }

                if (!qFuzzyIsNull(delta)) {
                    const qreal factor = std::pow(1.0015, delta);
                    owner_->applyZoomAtViewportPosition(factor, wheelEvent->position());
                }

                event->accept();
                return true;
            }

            QPoint panDelta = wheelEvent->pixelDelta();
            if (panDelta.isNull()) {
                const QPoint angleDelta = wheelEvent->angleDelta();
                panDelta = QPoint(qRound(angleDelta.x() / 4.0), qRound(angleDelta.y() / 4.0));
            }

            if (!panDelta.isNull()) {
                if (owner_->mapView_->horizontalScrollBar() != nullptr) {
                    owner_->mapView_->horizontalScrollBar()->setValue(owner_->mapView_->horizontalScrollBar()->value() - panDelta.x());
                }
                if (owner_->mapView_->verticalScrollBar() != nullptr) {
                    owner_->mapView_->verticalScrollBar()->setValue(owner_->mapView_->verticalScrollBar()->value() - panDelta.y());
                }

                owner_->autoFitEnabled_ = false;
                owner_->syncZoomFactorFromView();
                owner_->updateCommandSurfaceState();
            }

            event->accept();
            return true;
        }
        case QEvent::NativeGesture: {
            auto *gestureEvent = static_cast<QNativeGestureEvent *>(event);
            if (owner_->primaryPointerInteractionActive_) {
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::BeginNativeGesture) {
                owner_->nativeZoomGestureActive_ = true;
                owner_->lastNativeZoomGestureUtc_ = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::EndNativeGesture) {
                owner_->nativeZoomGestureActive_ = false;
                owner_->lastNativeZoomGestureUtc_ = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
                owner_->nativeZoomGestureActive_ = true;
                owner_->lastNativeZoomGestureUtc_ = QDateTime::currentDateTimeUtc();
                const qreal rawValue = gestureEvent->value();
                if (!std::isfinite(rawValue)) {
                    event->accept();
                    return true;
                }

                // Clamp one pinch update so trackpad spikes cannot jump straight to extreme scales.
                const qreal clampedDelta = qBound(-0.35, rawValue, 0.35);
                const qreal factor = std::exp(clampedDelta);
                if (factor > 0.0) {
                    owner_->applyZoomAtViewportPosition(factor, gestureEvent->position());
                }
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::TouchBegin: {
            if (!shouldEnableTouchPanCandidate(owner_->touchFriendlyControlsEnabled_,
                                               owner_->selectModeActive_,
                                               owner_->primaryPointerInteractionActive_)) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() == 2) {
                const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
                owner_->touchPanCandidate_ = true;
                owner_->touchPanActive_ = false;
                owner_->touchPanStartPosition_ = centroid;
                owner_->touchPanLastPosition_ = centroid;
            }
            break;
        }
        case QEvent::TouchUpdate: {
            if (!owner_->touchPanCandidate_ || owner_->primaryPointerInteractionActive_) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() != 2) {
                owner_->touchPanCandidate_ = false;
                owner_->touchPanActive_ = false;
                break;
            }

            const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
            if (!owner_->touchPanActive_) {
                const qreal threshold = 8.0;
                if (QLineF(owner_->touchPanStartPosition_, centroid).length() < threshold) {
                    event->accept();
                    return true;
                }
                owner_->touchPanActive_ = true;
            }

            const QPointF delta = centroid - owner_->touchPanLastPosition_;
            owner_->touchPanLastPosition_ = centroid;
            if (owner_->mapView_->horizontalScrollBar() != nullptr) {
                owner_->mapView_->horizontalScrollBar()->setValue(owner_->mapView_->horizontalScrollBar()->value() - qRound(delta.x()));
            }
            if (owner_->mapView_->verticalScrollBar() != nullptr) {
                owner_->mapView_->verticalScrollBar()->setValue(owner_->mapView_->verticalScrollBar()->value() - qRound(delta.y()));
            }

            owner_->autoFitEnabled_ = false;
            owner_->syncZoomFactorFromView();
            owner_->updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            owner_->touchPanCandidate_ = false;
            owner_->touchPanActive_ = false;
            break;
        case QEvent::Leave:
            if (owner_->mapPanActive_) {
                owner_->mapPanActive_ = false;
                viewport->unsetCursor();
            }
            owner_->primaryPointerInteractionActive_ = false;
            owner_->touchPanCandidate_ = false;
            owner_->touchPanActive_ = false;
            owner_->nativeZoomGestureActive_ = false;
            owner_->interactiveDrawStrokeActive_ = false;
            owner_->interactiveDrawAnchorPressActive_ = false;
            owner_->interactiveDrawAnchorDragActive_ = false;
            owner_->interactiveDrawControlDragActive_ = false;
            viewport->unsetCursor();
            if (owner_->interactiveDrawHoverActive_) {
                owner_->interactiveDrawHoverActive_ = false;
                owner_->updateInteractiveDrawPreview();
            }
            break;
        case QEvent::Resize:
            if (owner_->autoFitEnabled_ && owner_->mapView_->isVisible()) {
                owner_->fitMapToView(owner_->fitBackgroundRequested_);
            }
            break;
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area
                || owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Freehand) {
                if ((keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete)
                    && keyEvent->modifiers() == Qt::NoModifier
                    && owner_->interactiveDrawMode_ != MapEditorTab::InteractiveDrawMode::Freehand) {
                    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Line
                        && !owner_->interactiveDrawLineVertices_.isEmpty()) {
                        owner_->interactiveDrawLineVertices_.removeLast();
                        if (!owner_->interactiveDrawLineVertices_.isEmpty()) {
                            MapEditorInteractiveLineDraftVertex &tail = owner_->interactiveDrawLineVertices_.last();
                            tail.outgoingControlScene.reset();
                            tail.outgoingControlSource.reset();
                        }
                        owner_->updateInteractiveDrawPreview();
                        owner_->toolbarStatusNote_ = owner_->tr("Vertex removed from current draft (%1 remaining).")
                                                 .arg(owner_->interactiveDrawLineVertices_.size());
                        owner_->refreshToolbarSummary();
                        owner_->updateCommandSurfaceState();
                        event->accept();
                        return true;
                    }
                    if (owner_->interactiveDrawMode_ == MapEditorTab::InteractiveDrawMode::Area
                        && !owner_->interactiveDrawLineVertices_.isEmpty()) {
                        owner_->interactiveDrawLineVertices_.removeLast();
                        if (!owner_->interactiveDrawLineVertices_.isEmpty()) {
                            MapEditorInteractiveLineDraftVertex &tail = owner_->interactiveDrawLineVertices_.last();
                            tail.outgoingControlScene.reset();
                            tail.outgoingControlSource.reset();
                        }
                        owner_->updateInteractiveDrawPreview();
                        owner_->toolbarStatusNote_ = owner_->tr("Vertex removed from current draft (%1 remaining).")
                                                 .arg(owner_->interactiveDrawLineVertices_.size());
                        owner_->refreshToolbarSummary();
                        owner_->updateCommandSurfaceState();
                        event->accept();
                        return true;
                    }
                }
            }

            const bool insertShortcut = keyEvent->key() == Qt::Key_Insert
                || (keyEvent->key() == Qt::Key_I && keyEvent->modifiers() == Qt::NoModifier);
            if (insertShortcut) {
                if (owner_->insertLineVertexFromSelection()) {
                    event->accept();
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
                if (owner_->removeLineVertexFromSelection()) {
                    event->accept();
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_S && keyEvent->modifiers() == Qt::NoModifier) {
                if (owner_->toggleLineVertexSmoothFromSelection()) {
                    event->accept();
                    return true;
                }
            }
            break;
        }
        default:
            break;
        }
    } else if (watched == owner_->mapView_ && event->type() == QEvent::Resize) {
        if (owner_->autoFitEnabled_ && owner_->mapView_->isVisible()) {
            owner_->fitMapToView(owner_->fitBackgroundRequested_);
        }
    }

    return std::nullopt;
}
}
