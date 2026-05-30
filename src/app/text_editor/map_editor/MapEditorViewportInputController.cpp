#include "MapEditorViewportInputController.h"

#include <QCoreApplication>

#include "MapEditorInputPolicy.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QEvent>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QTransform>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

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
        if (geometryKind.startsWith(QStringLiteral("line control"))) {
            return 0;
        }
        if (geometryKind == QStringLiteral("line") || geometryKind == QStringLiteral("area")) {
            return 1;
        }
    }

    const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
    switch (subtype) {
    case kMapSceneSelectionSubtypeLineControl:
        return 0;
    case kMapSceneSelectionSubtypeLineAnchor:
    case kMapSceneSelectionSubtypeAreaVertex:
        return 1;
    case kMapSceneSelectionSubtypePointOrientationHandle:
        return 1;
    case kMapSceneSelectionSubtypeLineControlConnector:
        return 3;
    case kMapSceneSelectionSubtypeLineDetail:
        return 4;
    case kMapSceneSelectionSubtypeAreaFill:
        return 5;
    default:
        break;
    }

    return 1;
}

qreal itemUnitToViewPixels(const QGraphicsItem *item, const QTransform &viewTransform)
{
    if (item == nullptr) {
        return 1.0;
    }

    const QPointF originScene = item->mapToScene(QPointF(0.0, 0.0));
    const QPointF xScene = item->mapToScene(QPointF(1.0, 0.0));
    const QPointF yScene = item->mapToScene(QPointF(0.0, 1.0));
    const QPointF originView = viewTransform.map(originScene);
    const QPointF xDelta = viewTransform.map(xScene) - originView;
    const QPointF yDelta = viewTransform.map(yScene) - originView;
    const qreal xScale = std::hypot(xDelta.x(), xDelta.y());
    const qreal yScale = std::hypot(yDelta.x(), yDelta.y());
    return qMax<qreal>(0.001, qMax(xScale, yScale));
}

bool genericPathItemContainsStrokedHit(const QGraphicsItem *item,
                                       const QPointF &scenePosition,
                                       const QTransform &viewTransform)
{
    const auto *pathItem = dynamic_cast<const QGraphicsPathItem *>(item);
    if (pathItem == nullptr) {
        return true;
    }

    const QPainterPath path = pathItem->path();
    const QPointF localPosition = pathItem->mapFromScene(scenePosition);
    const qreal viewScale = itemUnitToViewPixels(pathItem, viewTransform);
    const qreal strokeRadiusPixels = pathItem->pen().widthF() * 0.5;
    const qreal tolerance = (strokeRadiusPixels + 4.0) / viewScale;
    if (!path.boundingRect().adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(localPosition)) {
        return false;
    }

    const qreal pathLength = path.length();
    if (pathLength <= 0.0) {
        return false;
    }

    const int sampleCount = qBound(24, static_cast<int>(std::ceil(pathLength / std::max<qreal>(tolerance * 0.5, 2.0))), 512);
    for (int index = 0; index <= sampleCount; ++index) {
        const qreal percent = static_cast<qreal>(index) / static_cast<qreal>(sampleCount);
        const QPointF pathPoint = path.pointAtPercent(percent);
        const qreal dx = pathPoint.x() - localPosition.x();
        const qreal dy = pathPoint.y() - localPosition.y();
        if (std::hypot(dx, dy) <= tolerance) {
            return true;
        }
    }
    return false;
}

QGraphicsItem *preferredMapHitItem(const QList<QGraphicsItem *> &hitItems,
                                   bool requireSelected = false,
                                   std::optional<QPointF> scenePosition = std::nullopt,
                                   const QTransform &viewTransform = QTransform())
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

        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        if (scenePosition.has_value()
            && subtype == kMapSceneSelectionSubtypeGeneric
            && !genericPathItemContainsStrokedHit(item, scenePosition.value(), viewTransform)) {
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

MapEditorInteractiveDrawMode MapEditorViewportInputController::drawMode() const
{
    return context_.drawMode ? context_.drawMode() : MapEditorInteractiveDrawMode::None;
}

QString MapEditorViewportInputController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorViewportInputController", text);
}

MapEditorViewportInputController::MapEditorViewportInputController(MapEditorViewportInputContext context)
    : context_(std::move(context))
{
}

std::optional<bool> MapEditorViewportInputController::handleEvent(QObject *watched, QEvent *event)
{
    if (context_.view == nullptr) {
        return std::nullopt;
    }

    QWidget *viewport = context_.view->viewport();
    if (watched == viewport) {
        switch (event->type()) {
        case QEvent::TabletPress:
        case QEvent::TabletMove:
        case QEvent::TabletRelease:
            (*context_.lastTabletInteractionUtc) = QDateTime::currentDateTimeUtc();
            break;
        case QEvent::MouseButtonPress: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                if (drawMode() == MapEditorInteractiveDrawMode::Freehand) {
                    if (context_.textEditor == nullptr) {
                        (*context_.toolbarStatusNote) = tr("Drawing failed: no active TH2 text editor.");
                        context_.refreshToolbarSummary();
                        event->accept();
                        return true;
                    }

                    context_.clearInteractiveDrawSession(false);
                    const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                    (*context_.interactiveDrawStrokeActive) = true;
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(scenePoint));
                    (*context_.interactiveDrawSceneVertices).append(scenePoint);
                    context_.updateInteractiveDrawPreview();
                    (*context_.toolbarStatusNote) = tr("Freehand mode: drawing stroke...");
                    context_.refreshToolbarSummary();
                    context_.updateCommandSurfaceState();
                    (*context_.primaryPointerInteractionActive) = false;
                    event->accept();
                    return true;
                }
                if (drawMode() == MapEditorInteractiveDrawMode::Line
                    || drawMode() == MapEditorInteractiveDrawMode::Area) {
                    const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                    const QPointF sceneOffset = context_.view->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (const auto handle = context_.interactiveLineControlAt(scenePoint, controlHitRadius)) {
                        (*context_.interactiveDrawControlDragActive) = true;
                        (*context_.interactiveDrawControlDragHandle) = handle.value();
                        (*context_.interactiveDrawAnchorPressActive) = false;
                        (*context_.interactiveDrawAnchorDragActive) = false;
                        (*context_.interactiveDrawHoverActive) = false;
                        viewport->setCursor(Qt::ClosedHandCursor);
                        (*context_.toolbarStatusNote) = drawMode() == MapEditorInteractiveDrawMode::Line
                            ? tr("Line mode: dragging bezier control point.")
                            : tr("Area mode: dragging bezier control point.");
                        context_.refreshToolbarSummary();
                        context_.updateCommandSurfaceState();
                        (*context_.primaryPointerInteractionActive) = false;
                        event->accept();
                        return true;
                    }

                    (*context_.interactiveDrawAnchorPressActive) = true;
                    (*context_.interactiveDrawAnchorPressScenePoint) = scenePoint;
                    (*context_.interactiveDrawAnchorDragActive) = false;
                    (*context_.interactiveDrawAnchorDragScenePoint) = (*context_.interactiveDrawAnchorPressScenePoint);
                    (*context_.interactiveDrawControlDragActive) = false;
                    (*context_.interactiveDrawHoverActive) = false;
                    context_.updateInteractiveDrawPreview();
                    (*context_.primaryPointerInteractionActive) = false;
                    event->accept();
                    return true;
                }
                if (context_.handleInteractiveDrawClick(context_.view->mapToScene(mouseEvent->pos()))) {
                    (*context_.primaryPointerInteractionActive) = false;
                    event->accept();
                    return true;
                }
                (*context_.primaryPointerInteractionActive) = true;
                if (context_.view != nullptr) {
                    (*context_.pendingClickSelection) = true;
                    (*context_.pendingClickScenePosition) = context_.view->mapToScene(mouseEvent->pos());
                    (*context_.pendingClickElapsed).start();
                    (*context_.pendingClickLineNumber) = 0;
                    (*context_.pendingClickSourceVertexIndex) = -1;
                    (*context_.pendingClickGeometryKind).clear();
                    if (context_.scene != nullptr) {
                        const QList<QGraphicsItem *> hitItems = context_.scene->items((*context_.pendingClickScenePosition),
                                                                                 Qt::IntersectsItemShape,
                                                                                 Qt::DescendingOrder,
                                                                                 context_.view->transform());
                        if (QGraphicsItem *item = preferredMapHitItem(hitItems,
                                                                      false,
                                                                      (*context_.pendingClickScenePosition),
                                                                      context_.view->transform())) {
                            const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
                            (*context_.pendingClickLineNumber) = lineNumber;
                            if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item)) {
                                (*context_.pendingClickSourceVertexIndex) = vertexItem->vertexIndex();
                                (*context_.pendingClickGeometryKind) = vertexItem->geometryKind();
                            } else if (item->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineControlConnector) {
                                const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
                                if (ownerVertexIndex >= 0) {
                                    (*context_.pendingClickSourceVertexIndex) = ownerVertexIndex;
                                    (*context_.pendingClickGeometryKind) = QStringLiteral("line");
                                }
                            } else if (dynamic_cast<QGraphicsPathItem *>(item) != nullptr) {
                                const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
                                if (subtype == kMapSceneSelectionSubtypeGeneric
                                    || subtype == kMapSceneSelectionSubtypeAreaFill) {
                                    context_.scene->clearSelection();
                                    item->setSelected(true);
                                    event->accept();
                                    return true;
                                }
                            }
                        }
                    }
                }
            }

            if (mouseEvent->button() == Qt::RightButton) {
                (*context_.mapPanActive) = true;
                (*context_.mapPanLastPosition) = mouseEvent->pos();
                viewport->setCursor(Qt::ClosedHandCursor);
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::MouseMove: {
            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawControlDragActive)) {
                const QPointF scenePoint = context_.view->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                if (context_.setInteractiveLineControlScenePoint((*context_.interactiveDrawControlDragHandle), scenePoint)) {
                    context_.updateInteractiveDrawPreview();
                }
                event->accept();
                return true;
            }

            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawAnchorPressActive)) {
                const QPointF scenePoint = context_.view->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal dragThreshold = 4.0;
                if (!(*context_.interactiveDrawAnchorDragActive)
                    && QLineF((*context_.interactiveDrawAnchorPressScenePoint), scenePoint).length() >= dragThreshold) {
                    (*context_.interactiveDrawAnchorDragActive) = true;
                }
                (*context_.interactiveDrawAnchorDragScenePoint) = scenePoint;
                context_.updateInteractiveDrawPreview();
                event->accept();
                return true;
            }

            if (drawMode() == MapEditorInteractiveDrawMode::Freehand && (*context_.interactiveDrawStrokeActive)) {
                const QPointF scenePoint = context_.view->mapToScene(static_cast<QMouseEvent *>(event)->pos());
                constexpr qreal minimumSceneSampleDistance = 4.0;
                if ((*context_.interactiveDrawSceneVertices).isEmpty()
                    || QLineF((*context_.interactiveDrawSceneVertices).last(), scenePoint).length() >= minimumSceneSampleDistance) {
                    (*context_.interactiveDrawSceneVertices).append(scenePoint);
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(scenePoint));
                    context_.updateInteractiveDrawPreview();
                    context_.updateCommandSurfaceState();
                }
                event->accept();
                return true;
            }

            if (drawMode() == MapEditorInteractiveDrawMode::Line
                || drawMode() == MapEditorInteractiveDrawMode::Area) {
                const bool hasDraftVertices = !(*context_.interactiveDrawLineVertices).isEmpty();
                if (hasDraftVertices) {
                    const QPoint mousePosition = static_cast<QMouseEvent *>(event)->pos();
                    const QPointF scenePoint = context_.view->mapToScene(mousePosition);
                    const QPointF sceneOffset = context_.view->mapToScene(mousePosition + QPoint(8, 0));
                    const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                    if (context_.interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                        viewport->setCursor(Qt::OpenHandCursor);
                    } else {
                        viewport->unsetCursor();
                    }
                    (*context_.interactiveDrawHoverActive) = true;
                    (*context_.interactiveDrawHoverScenePoint) = scenePoint;
                    context_.updateInteractiveDrawPreview();
                    event->accept();
                    return true;
                }
            }

            if (!(*context_.mapPanActive)) {
                break;
            }

            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            const QPoint delta = mouseEvent->pos() - (*context_.mapPanLastPosition);
            (*context_.mapPanLastPosition) = mouseEvent->pos();
            if (context_.view->horizontalScrollBar() != nullptr) {
                context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - delta.x());
            }
            if (context_.view->verticalScrollBar() != nullptr) {
                context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - delta.y());
            }

            (*context_.autoFitEnabled) = false;
            context_.syncZoomFactorFromView();
            context_.updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::MouseButtonRelease: {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawControlDragActive)
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF scenePoint = context_.view->mapToScene(mouseEvent->pos());
                context_.setInteractiveLineControlScenePoint((*context_.interactiveDrawControlDragHandle), scenePoint);
                (*context_.interactiveDrawControlDragActive) = false;
                const QPointF sceneOffset = context_.view->mapToScene(mouseEvent->pos() + QPoint(8, 0));
                const qreal controlHitRadius = std::max<qreal>(4.0, QLineF(scenePoint, sceneOffset).length());
                if (context_.interactiveLineControlAt(scenePoint, controlHitRadius).has_value()) {
                    viewport->setCursor(Qt::OpenHandCursor);
                } else {
                    viewport->unsetCursor();
                }
                (*context_.toolbarStatusNote) = drawMode() == MapEditorInteractiveDrawMode::Line
                    ? tr("Line mode: bezier control adjusted.")
                    : tr("Area mode: bezier control adjusted.");
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if ((drawMode() == MapEditorInteractiveDrawMode::Line
                 || drawMode() == MapEditorInteractiveDrawMode::Area)
                && (*context_.interactiveDrawAnchorPressActive)
                && mouseEvent->button() == Qt::LeftButton) {
                const MapEditorInteractiveDrawMode currentDrawMode = drawMode();
                const QPointF anchorScenePoint = (*context_.interactiveDrawAnchorPressScenePoint);
                const QPointF releaseScenePoint = context_.view->mapToScene(mouseEvent->pos());
                std::optional<QPointF> dragScenePoint;
                if ((*context_.interactiveDrawAnchorDragActive)) {
                    constexpr qreal dragThreshold = 4.0;
                    if (QLineF((*context_.interactiveDrawAnchorPressScenePoint), releaseScenePoint).length() >= dragThreshold) {
                        dragScenePoint = releaseScenePoint;
                    }
                }

                bool closeLineDraftByClick = false;
                if (currentDrawMode == MapEditorInteractiveDrawMode::Line
                    && !(*context_.interactiveDrawAnchorDragActive)
                    && context_.interactiveDrawLineVertices != nullptr
                    && context_.interactiveDrawLineVertices->size() >= 2) {
                    const QPointF closeHitProbe = context_.view->mapToScene(mouseEvent->pos() + QPoint(10, 0));
                    const qreal closeHitRadius = std::max<qreal>(5.0, QLineF(releaseScenePoint, closeHitProbe).length());
                    const QPointF firstAnchorScenePoint = context_.interactiveDrawLineVertices->first().anchorScene;
                    closeLineDraftByClick = QLineF(releaseScenePoint, firstAnchorScenePoint).length() <= closeHitRadius;
                }

                (*context_.interactiveDrawAnchorPressActive) = false;
                (*context_.interactiveDrawAnchorDragActive) = false;
                (*context_.interactiveDrawHoverActive) = false;

                if (closeLineDraftByClick && context_.commitInteractiveDrawSession != nullptr) {
                    context_.commitInteractiveDrawSession(true);
                    event->accept();
                    return true;
                }

                context_.captureInteractiveLineAnchor(anchorScenePoint, dragScenePoint);
                (*context_.toolbarStatusNote) = currentDrawMode == MapEditorInteractiveDrawMode::Line
                    ? tr("Line mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg((*context_.interactiveDrawLineVertices).size())
                    : tr("Area mode: %1 vertex/vertices captured. Press Enter or Complete Draft.")
                          .arg((*context_.interactiveDrawLineVertices).size());
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                event->accept();
                return true;
            }
            if (drawMode() == MapEditorInteractiveDrawMode::Freehand
                && (*context_.interactiveDrawStrokeActive)
                && mouseEvent->button() == Qt::LeftButton) {
                const QPointF releasePoint = context_.view->mapToScene(mouseEvent->pos());
                if ((*context_.interactiveDrawSceneVertices).isEmpty()
                    || QLineF((*context_.interactiveDrawSceneVertices).last(), releasePoint).length() >= 1.0) {
                    (*context_.interactiveDrawSceneVertices).append(releasePoint);
                    (*context_.interactiveDrawSourceVertices).append(context_.sourcePointFromScenePosition(releasePoint));
                }
                (*context_.interactiveDrawStrokeActive) = false;

                if ((*context_.interactiveDrawSourceVertices).size() < 2) {
                    context_.clearInteractiveDrawSession(false);
                    (*context_.toolbarStatusNote) = tr("Freehand mode needs a drag stroke to create a line.");
                    context_.refreshToolbarSummary();
                    context_.updateCommandSurfaceState();
                    event->accept();
                    return true;
                }

                const bool committed = context_.commitInteractiveDrawVertices(QStringLiteral("line"),
                                                                     (*context_.interactiveDrawSourceVertices),
                                                                     tr("freehand line"));
                context_.clearInteractiveDrawSession(false);
                if (committed) {
                    context_.updateHelpPanel();
                }
                context_.refreshToolbarSummary();
                context_.updateCommandSurfaceState();
                event->accept();
                return true;
            }

            if (mouseEvent->button() == Qt::LeftButton && mouseEvent->buttons() == Qt::NoButton) {
                (*context_.primaryPointerInteractionActive) = false;
            }

            if ((*context_.mapPanActive) && mouseEvent->button() == Qt::RightButton) {
                (*context_.mapPanActive) = false;
                viewport->unsetCursor();
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::Wheel: {
            auto *wheelEvent = static_cast<QWheelEvent *>(event);
            if ((*context_.nativeZoomGestureActive)
                && (*context_.lastNativeZoomGestureUtc).isValid()
                && (*context_.lastNativeZoomGestureUtc).msecsTo(QDateTime::currentDateTimeUtc()) > 1500) {
                (*context_.nativeZoomGestureActive) = false;
            }

            const bool recentNativeZoom = (*context_.lastNativeZoomGestureUtc).isValid()
                && (*context_.lastNativeZoomGestureUtc).msecsTo(QDateTime::currentDateTimeUtc()) <= 150;
            if ((*context_.nativeZoomGestureActive) || recentNativeZoom) {
                event->accept();
                return true;
            }

            if ((*context_.primaryPointerInteractionActive)) {
                event->accept();
                return true;
            }

            const Qt::KeyboardModifiers modifiers = wheelEvent->modifiers();
            const bool cmdModifier = modifiers.testFlag(Qt::ControlModifier) || modifiers.testFlag(Qt::MetaModifier);
            const bool preciseScroll = wheelEventHasPreciseScrollingDeltas(wheelEvent);
            const MapEditorWheelAction wheelAction = resolveMapEditorWheelAction((*context_.touchFriendlyControlsEnabled),
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
                    context_.applyZoomAtViewportPosition(factor, wheelEvent->position());
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
                if (context_.view->horizontalScrollBar() != nullptr) {
                    context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - panDelta.x());
                }
                if (context_.view->verticalScrollBar() != nullptr) {
                    context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - panDelta.y());
                }

                (*context_.autoFitEnabled) = false;
                context_.syncZoomFactorFromView();
                context_.updateCommandSurfaceState();
            }

            event->accept();
            return true;
        }
        case QEvent::NativeGesture: {
            auto *gestureEvent = static_cast<QNativeGestureEvent *>(event);
            if ((*context_.primaryPointerInteractionActive)) {
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::BeginNativeGesture) {
                (*context_.nativeZoomGestureActive) = true;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::EndNativeGesture) {
                (*context_.nativeZoomGestureActive) = false;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                event->accept();
                return true;
            }

            if (gestureEvent->gestureType() == Qt::ZoomNativeGesture) {
                (*context_.nativeZoomGestureActive) = true;
                (*context_.lastNativeZoomGestureUtc) = QDateTime::currentDateTimeUtc();
                const qreal rawValue = gestureEvent->value();
                if (!std::isfinite(rawValue)) {
                    event->accept();
                    return true;
                }

                // Clamp one pinch update so trackpad spikes cannot jump straight to extreme scales.
                const qreal clampedDelta = qBound(-0.35, rawValue, 0.35);
                const qreal factor = std::exp(clampedDelta);
                if (factor > 0.0) {
                    context_.applyZoomAtViewportPosition(factor, gestureEvent->position());
                }
                event->accept();
                return true;
            }
            break;
        }
        case QEvent::TouchBegin: {
            if (!shouldEnableTouchPanCandidate((*context_.touchFriendlyControlsEnabled),
                                               (*context_.selectModeActive),
                                               (*context_.primaryPointerInteractionActive))) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() == 2) {
                const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
                (*context_.touchPanCandidate) = true;
                (*context_.touchPanActive) = false;
                (*context_.touchPanStartPosition) = centroid;
                (*context_.touchPanLastPosition) = centroid;
            }
            break;
        }
        case QEvent::TouchUpdate: {
            if (!(*context_.touchPanCandidate) || (*context_.primaryPointerInteractionActive)) {
                event->accept();
                return true;
            }

            auto *touchEvent = static_cast<QTouchEvent *>(event);
            if (touchEvent->points().size() != 2) {
                (*context_.touchPanCandidate) = false;
                (*context_.touchPanActive) = false;
                break;
            }

            const QPointF centroid = (touchEvent->points().at(0).position() + touchEvent->points().at(1).position()) / 2.0;
            if (!(*context_.touchPanActive)) {
                const qreal threshold = 8.0;
                if (QLineF((*context_.touchPanStartPosition), centroid).length() < threshold) {
                    event->accept();
                    return true;
                }
                (*context_.touchPanActive) = true;
            }

            const QPointF delta = centroid - (*context_.touchPanLastPosition);
            (*context_.touchPanLastPosition) = centroid;
            if (context_.view->horizontalScrollBar() != nullptr) {
                context_.view->horizontalScrollBar()->setValue(context_.view->horizontalScrollBar()->value() - qRound(delta.x()));
            }
            if (context_.view->verticalScrollBar() != nullptr) {
                context_.view->verticalScrollBar()->setValue(context_.view->verticalScrollBar()->value() - qRound(delta.y()));
            }

            (*context_.autoFitEnabled) = false;
            context_.syncZoomFactorFromView();
            context_.updateCommandSurfaceState();
            event->accept();
            return true;
        }
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            (*context_.touchPanCandidate) = false;
            (*context_.touchPanActive) = false;
            break;
        case QEvent::Leave:
            if ((*context_.mapPanActive)) {
                (*context_.mapPanActive) = false;
                viewport->unsetCursor();
            }
            (*context_.primaryPointerInteractionActive) = false;
            (*context_.touchPanCandidate) = false;
            (*context_.touchPanActive) = false;
            (*context_.nativeZoomGestureActive) = false;
            (*context_.interactiveDrawStrokeActive) = false;
            (*context_.interactiveDrawAnchorPressActive) = false;
            (*context_.interactiveDrawAnchorDragActive) = false;
            (*context_.interactiveDrawControlDragActive) = false;
            viewport->unsetCursor();
            if ((*context_.interactiveDrawHoverActive)) {
                (*context_.interactiveDrawHoverActive) = false;
                context_.updateInteractiveDrawPreview();
            }
            break;
        case QEvent::Resize:
            if ((*context_.autoFitEnabled) && context_.view->isVisible()) {
                context_.fitMapToView((*context_.fitBackgroundRequested));
            }
            break;
        case QEvent::KeyPress: {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (drawMode() == MapEditorInteractiveDrawMode::Line
                || drawMode() == MapEditorInteractiveDrawMode::Area
                || drawMode() == MapEditorInteractiveDrawMode::Freehand) {
                if ((keyEvent->key() == Qt::Key_Backspace || keyEvent->key() == Qt::Key_Delete)
                    && keyEvent->modifiers() == Qt::NoModifier
                    && drawMode() != MapEditorInteractiveDrawMode::Freehand) {
                    if (drawMode() == MapEditorInteractiveDrawMode::Line
                        && !(*context_.interactiveDrawLineVertices).isEmpty()) {
                        (*context_.interactiveDrawLineVertices).removeLast();
                        if (!(*context_.interactiveDrawLineVertices).isEmpty()) {
                            MapEditorInteractiveLineDraftVertex &tail = (*context_.interactiveDrawLineVertices).last();
                            tail.outgoingControlScene.reset();
                            tail.outgoingControlSource.reset();
                        }
                        context_.updateInteractiveDrawPreview();
                        (*context_.toolbarStatusNote) = tr("Vertex removed from current draft (%1 remaining).")
                                                 .arg((*context_.interactiveDrawLineVertices).size());
                        context_.refreshToolbarSummary();
                        context_.updateCommandSurfaceState();
                        event->accept();
                        return true;
                    }
                    if (drawMode() == MapEditorInteractiveDrawMode::Area
                        && !(*context_.interactiveDrawLineVertices).isEmpty()) {
                        (*context_.interactiveDrawLineVertices).removeLast();
                        if (!(*context_.interactiveDrawLineVertices).isEmpty()) {
                            MapEditorInteractiveLineDraftVertex &tail = (*context_.interactiveDrawLineVertices).last();
                            tail.outgoingControlScene.reset();
                            tail.outgoingControlSource.reset();
                        }
                        context_.updateInteractiveDrawPreview();
                        (*context_.toolbarStatusNote) = tr("Vertex removed from current draft (%1 remaining).")
                                                 .arg((*context_.interactiveDrawLineVertices).size());
                        context_.refreshToolbarSummary();
                        context_.updateCommandSurfaceState();
                        event->accept();
                        return true;
                    }
                }
            }

            const bool insertShortcut = keyEvent->key() == Qt::Key_Insert
                || (keyEvent->key() == Qt::Key_I && keyEvent->modifiers() == Qt::NoModifier);
            if (insertShortcut) {
                if (context_.insertLineVertexFromSelection()) {
                    event->accept();
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {
                if (context_.removeLineVertexFromSelection()) {
                    event->accept();
                    return true;
                }
            } else if (keyEvent->key() == Qt::Key_S && keyEvent->modifiers() == Qt::NoModifier) {
                if (context_.toggleLineVertexSmoothFromSelection()) {
                    event->accept();
                    return true;
                }
            }
            break;
        }
        default:
            break;
        }
    } else if (watched == context_.view && event->type() == QEvent::Resize) {
        if ((*context_.autoFitEnabled) && context_.view->isVisible()) {
            context_.fitMapToView((*context_.fitBackgroundRequested));
        }
    }

    return std::nullopt;
}
}
