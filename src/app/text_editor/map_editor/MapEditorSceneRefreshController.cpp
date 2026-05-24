#include "MapEditorSceneRefreshController.h"

#include "MapEditorInspectorData.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "../../../core/TherionDocumentParser.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QObject>
#include <QTransform>
#include <QUndoStack>

#include <optional>
#include <utility>

namespace TherionStudio
{
MapEditorSceneRefreshController::MapEditorSceneRefreshController(MapEditorSceneRefreshContext context)
    : context_(std::move(context))
{
}

QGraphicsScene *MapEditorSceneRefreshController::scene() const
{
    return context_.scene != nullptr ? *context_.scene : nullptr;
}

void MapEditorSceneRefreshController::buildMapScene()
{
    *context_.scene = new QGraphicsScene(context_.sceneParent);
    context_.view->setScene(*context_.scene);
    QObject::connect(*context_.scene,
                     &QGraphicsScene::selectionChanged,
                     context_.selectionConnectionContext,
                     [onSelectionChanged = context_.handleSceneSelectionChanged]() {
                         onSelectionChanged();
                     });
}

void MapEditorSceneRefreshController::refreshMapScene()
{
    if (scene() == nullptr) {
        return;
    }

    if (*context_.commandApplyInProgress) {
        *context_.sceneRefreshPending = true;
        return;
    }

    if (context_.undoStack != nullptr) {
        context_.undoStack->clear();
    }

    refreshMapScenePreservingUndoStack();
}

void MapEditorSceneRefreshController::refreshMapScenePreservingUndoStack()
{
    refreshMapScenePreservingUndoStack(false);
}

void MapEditorSceneRefreshController::refreshMapScenePreservingUndoStack(bool preserveViewport)
{
    QGraphicsScene *mapScene = scene();
    if (mapScene == nullptr) {
        return;
    }

    const bool canPreserveViewport = preserveViewport
        && context_.view != nullptr
        && context_.view->viewport() != nullptr;
    const QTransform preservedTransform = canPreserveViewport
        ? context_.view->transform()
        : QTransform();
    const QPointF preservedCenter = canPreserveViewport
        ? context_.view->mapToScene(context_.view->viewport()->rect().center())
        : QPointF();

    if (context_.undoStack != nullptr) {
        context_.updateCommandSurfaceState();
    }

    context_.clearMapScene();

    const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument
        ? context_.parsedLinesForCurrentDocument()
        : TherionDocumentParser::parseText(context_.documentText());
    const QVector<MapSceneEntry> entries = collectMapSceneEntries(parsedLines);
    QVector<MapGeometryFeature> geometryFeatures = collectGeometryFeatures(parsedLines);
    QHash<int, TherionParsedLine> parsedLinesByLineNumber;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        parsedLinesByLineNumber.insert(parsedLine.lineNumber, parsedLine);
    }
    for (MapGeometryFeature &feature : geometryFeatures) {
        if (feature.kind != MapGeometryFeature::Kind::Point) {
            continue;
        }
        const TherionParsedLine parsedLine = parsedLinesByLineNumber.value(feature.lineNumber);
        if (parsedLine.directive == QStringLiteral("point") && isOrientationSupportedForParsedLine(parsedLine)) {
            feature.orientationSupported = true;
            feature.orientationDegrees = pointOrientationFromParsedLine(parsedLine);
        }
    }
    const QRectF sourceBounds = context_.mapSourceBoundsForCurrentDocument();
    const std::optional<QRectF> sourceBoundsOverride = sourceBounds.isValid()
        ? std::optional<QRectF>(sourceBounds)
        : std::nullopt;
    const bool hasVisibleBackgroundBounds = context_.mapBackgroundFitBounds
        && context_.mapBackgroundFitBounds().isValid();
    const bool showEmptyDocumentGuides = !hasVisibleBackgroundBounds
        || !geometryFeatures.isEmpty()
        || !entries.isEmpty();
    renderMapWorkspaceScene(mapScene,
                            context_.filePath(),
                            entries,
                            geometryFeatures,
                            sourceBoundsOverride,
                            MapGridOptions{*context_.gridVisible,
                                           *context_.gridSpacingMeters,
                                           mapSourceUnitsPerMeterFromParsedLines(parsedLines)},
                            showEmptyDocumentGuides,
                            context_.itemsByLine,
                            context_.recordCardMove,
                            context_.recordCardVisibility,
                            context_.recordPointGeometryMove,
                            context_.recordLineAreaVertexMove,
                            context_.recordPointOrientationHandleChange,
                            context_.recordLinePointLeftHandleChange);

    context_.restoreBackgroundImageItems();
    context_.reprojectMetadataBackgroundLayersForCurrentDocument();
    context_.restoreDraftGeometryItems();
    context_.selectMapLine(context_.currentLineNumber(), !preserveViewport);
    context_.applyInspectorObjectVisibility();
    context_.updateGeometrySelectionPresentation();
    if (canPreserveViewport) {
        context_.view->setTransform(preservedTransform);
        context_.view->centerOn(preservedCenter);
        *context_.autoFitEnabled = false;
        context_.syncZoomFactorFromView();
    } else if (*context_.autoFitEnabled) {
        context_.fitMapToView(*context_.fitBackgroundRequested);
    } else {
        context_.syncZoomFactorFromView();
    }
    context_.updateInteractiveDrawPreview();
    context_.refreshStatus();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

void MapEditorSceneRefreshController::flushPendingMapSceneRefreshAfterCommand()
{
    if (!*context_.sceneRefreshPending) {
        return;
    }

    *context_.sceneRefreshPending = false;
    refreshMapScenePreservingUndoStack(true);
}

}
