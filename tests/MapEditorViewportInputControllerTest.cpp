#include "../src/app/text_editor/map_editor/MapEditorViewportInputController.h"
#include "../src/app/text_editor/map_editor/MapEditorSceneLifecycleController.h"

#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>

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
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    if (const int rc = runBackspaceDeleteOnMapViewAndViewportTest(); rc != 0) {
        return rc;
    }
    return runResizeAutoFitSuppressesCommandSurfaceUpdateTest();
}
