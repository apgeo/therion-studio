#include "../src/app/text_editor/map_editor/MapEditorViewportInputController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneLifecycleController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneInternals.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneSupport.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QMouseEvent>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runBackspaceDeleteOnMapViewAndViewportTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);

    int removeVertexCalls = 0;
    int deleteObjectCalls = 0;
    QString toolbarStatus;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.removeLineVertexFromSelection = [&removeVertexCalls]() {
        ++removeVertexCalls;
        return false;
    };
    context.deleteSelectedObjectFromSelection = [&deleteObjectCalls]() {
        ++deleteObjectCalls;
        return true;
    };

    MapEditorViewportInputController controller(context);

    QKeyEvent viewBackspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    const std::optional<bool> viewResult = controller.handleEvent(&view, &viewBackspace);
    if (!expect(viewResult.has_value() && viewResult.value(),
                "Backspace on focused map view should be handled by viewport input controller.")) {
        return 1;
    }
    if (!expect(removeVertexCalls == 1 && deleteObjectCalls == 1,
                "Backspace on map view should attempt vertex delete, then selected-object delete fallback.")) {
        return 1;
    }

    QKeyEvent viewportBackspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
    const std::optional<bool> viewportResult = controller.handleEvent(view.viewport(), &viewportBackspace);
    if (!expect(viewportResult.has_value() && viewportResult.value(),
                "Backspace on map viewport should be handled by viewport input controller.")) {
        return 1;
    }
    if (!expect(removeVertexCalls == 2 && deleteObjectCalls == 2,
                "Backspace on map viewport should attempt vertex delete, then selected-object delete fallback.")) {
        return 1;
    }

    QKeyEvent modifiedBackspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::MetaModifier);
    const std::optional<bool> modifiedResult = controller.handleEvent(&view, &modifiedBackspace);
    if (!expect(!modifiedResult.has_value(),
                "Backspace with modifiers should stay unhandled so platform shortcuts can propagate.")) {
        return 1;
    }
    if (!expect(removeVertexCalls == 2 && deleteObjectCalls == 2,
                "Backspace with modifiers should not trigger delete handlers.")) {
        return 1;
    }

    return 0;
}

int runResizeAutoFitSuppressesCommandSurfaceUpdateTest()
{
    QGraphicsScene scene;
    scene.addRect(QRectF(0.0, 0.0, 100.0, 100.0));
    QGraphicsView view(&scene);
    view.resize(240, 180);

    bool autoFitEnabled = true;
    qreal zoomFactor = 1.0;
    int commandSurfaceUpdates = 0;
    int zoomStatusUpdates = 0;
    QGraphicsScene *scenePointer = &scene;
    QHash<int, QGraphicsItem *> itemsByLine;
    QVector<QGraphicsRectItem *> draftGeometryItems;
    QVector<QGraphicsPixmapItem *> backgroundImageItems;
    bool interactiveDrawStrokeActive = false;
    QGraphicsPathItem *interactiveDrawPreviewPath = nullptr;
    QVector<QGraphicsItem *> interactiveDrawPreviewMarkers;
    bool updatingSelection = false;
    int nextDraftGeometryId = 1;
    int selectedBackgroundLayerIndex = -1;

    MapEditorSceneLifecycleContext context;
    context.scene = &scenePointer;
    context.view = &view;
    context.itemsByLine = &itemsByLine;
    context.draftGeometryItems = &draftGeometryItems;
    context.backgroundImageItems = &backgroundImageItems;
    context.interactiveDrawStrokeActive = &interactiveDrawStrokeActive;
    context.interactiveDrawPreviewPath = &interactiveDrawPreviewPath;
    context.interactiveDrawPreviewMarkers = &interactiveDrawPreviewMarkers;
    context.updatingSelection = &updatingSelection;
    context.nextDraftGeometryId = &nextDraftGeometryId;
    context.selectedBackgroundLayerIndex = &selectedBackgroundLayerIndex;
    context.autoFitEnabled = &autoFitEnabled;
    context.zoomFactor = &zoomFactor;
    context.mapBackgroundFitBounds = []() {
        return QRectF();
    };
    context.updateCommandSurfaceState = [&commandSurfaceUpdates]() {
        ++commandSurfaceUpdates;
    };
    context.zoomPercent = []() {
        return 100;
    };
    context.emitZoomStatusChanged = [&zoomStatusUpdates](int) {
        ++zoomStatusUpdates;
    };

    MapEditorSceneLifecycleController controller(context);
    controller.fitMapToView(false, false);
    if (!expect(commandSurfaceUpdates == 0,
                "Resize autofit lifecycle path should suppress command-surface updates.")) {
        return 1;
    }
    if (!expect(zoomStatusUpdates > 0,
                "Resize autofit lifecycle path should still synchronize zoom status.")) {
        return 1;
    }

    return 0;
}

int runSecondaryClickOpensContextMenuTest()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);
    view.resize(240, 180);

    QString toolbarStatus;
    bool touchFriendlyControlsEnabled = false;
    bool selectModeActive = true;
    bool autoFitEnabled = true;
    bool fitBackgroundRequested = false;
    bool mapPanActive = false;
    bool mapPanMoved = false;
    QPoint mapPanStartPosition;
    QPoint mapPanLastPosition;
    bool primaryPointerInteractionActive = false;
    bool touchPanCandidate = false;
    bool touchPanActive = false;
    QPointF touchPanStartPosition;
    QPointF touchPanLastPosition;
    QDateTime lastTabletInteractionUtc;
    bool nativeZoomGestureActive = false;
    QDateTime lastNativeZoomGestureUtc;
    bool pendingClickSelection = false;
    QPointF pendingClickScenePosition;
    QElapsedTimer pendingClickElapsed;
    int pendingClickLineNumber = 0;
    int pendingClickSourceVertexIndex = -1;
    QString pendingClickGeometryKind;
    QVector<QPointF> interactiveDrawSourceVertices;
    QVector<QPointF> interactiveDrawSceneVertices;
    QVector<MapEditorInteractiveLineDraftVertex> interactiveDrawLineVertices;
    bool interactiveDrawStrokeActive = false;
    bool interactiveDrawAnchorPressActive = false;
    QPointF interactiveDrawAnchorPressScenePoint;
    bool interactiveDrawAnchorDragActive = false;
    QPointF interactiveDrawAnchorDragScenePoint;
    bool interactiveDrawControlDragActive = false;
    MapEditorInteractiveLineControlHandleRef interactiveDrawControlDragHandle;
    bool interactiveDrawHoverActive = false;
    QPointF interactiveDrawHoverScenePoint;
    bool interactiveDrawHoverSnapActive = false;
    QPointF interactiveDrawHoverSnapScenePoint;
    int contextMenuCalls = 0;
    int preparedSelectionCalls = 0;
    int preparedLineNumber = 0;
    int preparedVertexIndex = -1;
    QString preparedGeometryKind;
    int commandSurfaceUpdates = 0;
    int zoomSyncs = 0;

    MapEditorViewportInputContext context;
    context.scene = &scene;
    context.view = &view;
    context.toolbarStatusNote = &toolbarStatus;
    context.touchFriendlyControlsEnabled = &touchFriendlyControlsEnabled;
    context.selectModeActive = &selectModeActive;
    context.autoFitEnabled = &autoFitEnabled;
    context.fitBackgroundRequested = &fitBackgroundRequested;
    context.mapPanActive = &mapPanActive;
    context.mapPanMoved = &mapPanMoved;
    context.mapPanStartPosition = &mapPanStartPosition;
    context.mapPanLastPosition = &mapPanLastPosition;
    context.primaryPointerInteractionActive = &primaryPointerInteractionActive;
    context.touchPanCandidate = &touchPanCandidate;
    context.touchPanActive = &touchPanActive;
    context.touchPanStartPosition = &touchPanStartPosition;
    context.touchPanLastPosition = &touchPanLastPosition;
    context.lastTabletInteractionUtc = &lastTabletInteractionUtc;
    context.nativeZoomGestureActive = &nativeZoomGestureActive;
    context.lastNativeZoomGestureUtc = &lastNativeZoomGestureUtc;
    context.pendingClickSelection = &pendingClickSelection;
    context.pendingClickScenePosition = &pendingClickScenePosition;
    context.pendingClickElapsed = &pendingClickElapsed;
    context.pendingClickLineNumber = &pendingClickLineNumber;
    context.pendingClickSourceVertexIndex = &pendingClickSourceVertexIndex;
    context.pendingClickGeometryKind = &pendingClickGeometryKind;
    context.interactiveDrawSourceVertices = &interactiveDrawSourceVertices;
    context.interactiveDrawSceneVertices = &interactiveDrawSceneVertices;
    context.interactiveDrawLineVertices = &interactiveDrawLineVertices;
    context.interactiveDrawStrokeActive = &interactiveDrawStrokeActive;
    context.interactiveDrawAnchorPressActive = &interactiveDrawAnchorPressActive;
    context.interactiveDrawAnchorPressScenePoint = &interactiveDrawAnchorPressScenePoint;
    context.interactiveDrawAnchorDragActive = &interactiveDrawAnchorDragActive;
    context.interactiveDrawAnchorDragScenePoint = &interactiveDrawAnchorDragScenePoint;
    context.interactiveDrawControlDragActive = &interactiveDrawControlDragActive;
    context.interactiveDrawControlDragHandle = &interactiveDrawControlDragHandle;
    context.interactiveDrawHoverActive = &interactiveDrawHoverActive;
    context.interactiveDrawHoverScenePoint = &interactiveDrawHoverScenePoint;
    context.interactiveDrawHoverSnapActive = &interactiveDrawHoverSnapActive;
    context.interactiveDrawHoverSnapScenePoint = &interactiveDrawHoverSnapScenePoint;
    context.drawMode = []() { return MapEditorInteractiveDrawMode::None; };
    context.showSelectionContextMenu = [&contextMenuCalls](const QPoint &) {
        ++contextMenuCalls;
    };
    context.prepareSelectionContextMenuState = [&preparedSelectionCalls,
                                                &preparedLineNumber,
                                                &preparedVertexIndex,
                                                &preparedGeometryKind](int lineNumber,
                                                                       int vertexIndex,
                                                                       const QString &geometryKind,
                                                                       const QPointF &) {
        ++preparedSelectionCalls;
        preparedLineNumber = lineNumber;
        preparedVertexIndex = vertexIndex;
        preparedGeometryKind = geometryKind;
    };
    context.syncZoomFactorFromView = [&zoomSyncs]() {
        ++zoomSyncs;
    };
    context.updateCommandSurfaceState = [&commandSurfaceUpdates]() {
        ++commandSurfaceUpdates;
    };

    MapEditorViewportInputController controller(context);

    const QPoint clickPosition(40, 40);
    const QPointF clickScenePosition = view.mapToScene(clickPosition);
    QGraphicsRectItem *hitItem = scene.addRect(QRectF(clickScenePosition.x() - 8.0,
                                                     clickScenePosition.y() - 8.0,
                                                     16.0,
                                                     16.0));
    hitItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
    hitItem->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    hitItem->setData(kMapSceneLineNumberRole, 77);

    QMouseEvent rightPress(QEvent::MouseButtonPress,
                           QPointF(clickPosition),
                           QPointF(view.viewport()->mapToGlobal(clickPosition)),
                           Qt::RightButton,
                           Qt::RightButton,
                           Qt::NoModifier);
    if (!expect(controller.handleEvent(view.viewport(), &rightPress).value_or(false),
                "Right mouse press should be handled as a map context-menu candidate.")) {
        return 1;
    }

    if (!expect(contextMenuCalls == 1,
                "Right mouse press should open the map selection context menu immediately.")) {
        return 1;
    }
    if (!expect(hitItem->isSelected() && pendingClickLineNumber == 77,
                "Right mouse press should select the map item under the cursor before opening the context menu.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77 && preparedVertexIndex == -1,
                "Right mouse press should prepare selection metadata before opening the context menu.")) {
        return 1;
    }
    if (!expect(!mapPanActive && !mapPanMoved,
                "Right click context-menu path should not leave pan state active.")) {
        return 1;
    }

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    controller.showContextMenuAtViewportPosition(clickPosition, view.viewport()->mapToGlobal(clickPosition));
    if (!expect(contextMenuCalls == 1,
                "Viewport custom context-menu request should open the map selection context menu.")) {
        return 1;
    }
    if (!expect(hitItem->isSelected() && pendingClickLineNumber == 77,
                "Viewport custom context-menu request should select the map item under the cursor.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77,
                "Viewport custom context-menu request should prepare selection metadata.")) {
        return 1;
    }

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    QMouseEvent controlClickPress(QEvent::MouseButtonPress,
                                  QPointF(clickPosition),
                                  QPointF(view.viewport()->mapToGlobal(clickPosition)),
                                  Qt::LeftButton,
                                  Qt::LeftButton,
                                  Qt::ControlModifier);
    if (!expect(controller.handleEvent(view.viewport(), &controlClickPress).value_or(false),
                "Control-click should be handled as a platform secondary click.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Control-click should open the map selection context menu.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77,
                "Control-click should prepare selection metadata before opening the context menu.")) {
        return 1;
    }

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    QContextMenuEvent trackpadContextMenu(QContextMenuEvent::Mouse,
                                          clickPosition,
                                          view.viewport()->mapToGlobal(clickPosition));
    if (!expect(controller.handleEvent(view.viewport(), &trackpadContextMenu).value_or(false),
                "Trackpad-style context menu event should be handled by the map viewport controller.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Trackpad-style context menu event should open the map selection context menu.")) {
        return 1;
    }
    if (!expect(hitItem->isSelected() && pendingClickLineNumber == 77,
                "Trackpad-style context menu event should select the map item under the cursor.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1 && preparedLineNumber == 77,
                "Trackpad-style context menu event should prepare selection metadata.")) {
        return 1;
    }

    const QPoint vertexClickPosition(90, 70);
    const QPointF vertexScenePosition = view.mapToScene(vertexClickPosition);
    const QRectF stableBounds(vertexScenePosition.x() - 100.0,
                              vertexScenePosition.y() - 100.0,
                              200.0,
                              200.0);
    auto *hiddenVertexItem = new MapEditableGeometryVertexItem(88,
                                                               QStringLiteral("line"),
                                                               7,
                                                               vertexScenePosition,
                                                               stableBounds,
                                                               stableBounds);
    hiddenVertexItem->setData(kMapSceneLineNumberRole, 88);
    hiddenVertexItem->setData(kMapSceneSelectionGatedRole, true);
    hiddenVertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
    hiddenVertexItem->setData(kMapSceneOwnerVertexRole, 7);
    hiddenVertexItem->setVisible(false);
    scene.addItem(hiddenVertexItem);

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    pendingClickSourceVertexIndex = -1;
    pendingClickGeometryKind.clear();
    QContextMenuEvent vertexTrackpadContextMenu(QContextMenuEvent::Mouse,
                                                vertexClickPosition,
                                                view.viewport()->mapToGlobal(vertexClickPosition));
    if (!expect(controller.handleEvent(view.viewport(), &vertexTrackpadContextMenu).value_or(false),
                "Trackpad-style context menu event on a gated line vertex should be handled.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Trackpad-style context menu event on a gated line vertex should open the context menu.")) {
        return 1;
    }
    if (!expect(hiddenVertexItem->isSelected() && pendingClickLineNumber == 88,
                "Trackpad-style context menu event should select the gated line vertex under the cursor.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1
                    && preparedLineNumber == 88
                    && preparedVertexIndex == 7
                    && preparedGeometryKind == QStringLiteral("line"),
                "Trackpad-style context menu event should prepare gated line vertex metadata.")) {
        return 1;
    }

    const QPoint pointClickPosition(130, 90);
    const QPointF pointScenePosition = view.mapToScene(pointClickPosition);
    const QRectF pointStableBounds(pointScenePosition.x() - 100.0,
                                   pointScenePosition.y() - 100.0,
                                   200.0,
                                   200.0);
    auto *nearbyVertexItem = new MapEditableGeometryVertexItem(98,
                                                               QStringLiteral("line"),
                                                               3,
                                                               pointScenePosition + QPointF(6.0, 0.0),
                                                               pointStableBounds,
                                                               pointStableBounds);
    nearbyVertexItem->setData(kMapSceneLineNumberRole, 98);
    nearbyVertexItem->setData(kMapSceneSelectionGatedRole, true);
    nearbyVertexItem->setData(kMapSceneSelectionSubtypeRole, kMapSceneSelectionSubtypeLineAnchor);
    nearbyVertexItem->setData(kMapSceneOwnerVertexRole, 3);
    scene.addItem(nearbyVertexItem);

    auto *nearbyPointItem = new MapEditablePointItem(99,
                                                     pointScenePosition,
                                                     pointStableBounds,
                                                     pointStableBounds);
    nearbyPointItem->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    nearbyPointItem->setData(kMapSceneLineNumberRole, 99);
    scene.addItem(nearbyPointItem);

    contextMenuCalls = 0;
    preparedSelectionCalls = 0;
    scene.clearSelection();
    pendingClickLineNumber = 0;
    pendingClickSourceVertexIndex = -1;
    pendingClickGeometryKind.clear();
    QContextMenuEvent pointNearVertexContextMenu(QContextMenuEvent::Mouse,
                                                 pointClickPosition,
                                                 view.viewport()->mapToGlobal(pointClickPosition));
    if (!expect(controller.handleEvent(view.viewport(), &pointNearVertexContextMenu).value_or(false),
                "Trackpad-style context menu event on a point near a line vertex should be handled.")) {
        return 1;
    }
    if (!expect(contextMenuCalls == 1,
                "Trackpad-style context menu event on a point near a line vertex should open the context menu.")) {
        return 1;
    }
    if (!expect(nearbyPointItem->isSelected() && !nearbyVertexItem->isSelected() && pendingClickLineNumber == 99,
                "Context menu selection should keep a directly hit point ahead of a nearby line vertex fallback.")) {
        return 1;
    }
    if (!expect(preparedSelectionCalls == 1
                    && preparedLineNumber == 99
                    && preparedVertexIndex == -1
                    && preparedGeometryKind.isEmpty(),
                "Context menu selection on a point near a line vertex should prepare point metadata.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int rc = runBackspaceDeleteOnMapViewAndViewportTest(); rc != 0) {
        return rc;
    }
    if (const int rc = runSecondaryClickOpensContextMenuTest(); rc != 0) {
        return rc;
    }
    return runResizeAutoFitSuppressesCommandSurfaceUpdateTest();
}
