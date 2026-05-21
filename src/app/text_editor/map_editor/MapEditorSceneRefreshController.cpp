#include "MapEditorSceneRefreshController.h"

#include "../TextEditorTab.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorTab.h"
#include "../../../core/TherionDocumentParser.h"

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QObject>
#include <QPointer>
#include <QUndoStack>

#include <optional>

namespace TherionStudio
{
MapEditorSceneRefreshController::MapEditorSceneRefreshController(MapEditorTab *owner)
    : owner_(owner)
{
}

void MapEditorSceneRefreshController::buildMapScene()
{
    owner_->mapScene_ = new QGraphicsScene(owner_);
    owner_->mapView_->setScene(owner_->mapScene_);
    QObject::connect(owner_->mapScene_, &QGraphicsScene::selectionChanged, owner_, &MapEditorTab::handleMapSceneSelectionChanged);
}

void MapEditorSceneRefreshController::refreshMapScene()
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    if (owner_->mapCommandApplyInProgress_) {
        owner_->mapSceneRefreshPending_ = true;
        return;
    }

    if (owner_->undoStack_ != nullptr) {
        owner_->undoStack_->clear();
    }

    refreshMapScenePreservingUndoStack();
}

void MapEditorSceneRefreshController::refreshMapScenePreservingUndoStack()
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    if (owner_->undoStack_ != nullptr) {
        owner_->updateCommandSurfaceState();
    }

    owner_->clearMapScene();

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(owner_->textEditor_->text());
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
    const QRectF sourceBounds = owner_->mapSourceBoundsForCurrentDocument();
    const std::optional<QRectF> sourceBoundsOverride = sourceBounds.isValid()
        ? std::optional<QRectF>(sourceBounds)
        : std::nullopt;
    const QPointer<MapEditorTab> self(owner_);
    renderMapWorkspaceScene(owner_->mapScene_,
                            owner_->filePath(),
                            entries,
                            geometryFeatures,
                            sourceBoundsOverride,
                            MapGridOptions{owner_->mapGridVisible_,
                                           owner_->mapGridSpacingMeters_,
                                           mapSourceUnitsPerMeterFromParsedLines(parsedLines)},
                            &owner_->mapItemsByLine_,
                            [owner = owner_](int lineNumber, const QPointF &oldPosition, const QPointF &newPosition) {
                                owner->recordCardMove(lineNumber, oldPosition, newPosition);
                            },
                            [owner = owner_](int lineNumber, bool oldVisible, bool newVisible) {
                                owner->recordCardVisibility(lineNumber, oldVisible, newVisible);
                            },
                            [owner = owner_](int lineNumber, const QPointF &oldPoint, const QPointF &newPoint) {
                                owner->recordPointGeometryMove(lineNumber, oldPoint, newPoint);
                            },
                            [owner = owner_](int lineNumber, const QString &kind, int vertexIndex, const QPointF &oldPoint, const QPointF &newPoint) {
                                owner->recordLineAreaVertexMove(lineNumber, kind, vertexIndex, oldPoint, newPoint);
                            },
                            [self](int lineNumber, qreal orientationDegrees) {
                                if (self != nullptr) {
                                    self->recordPointOrientationHandleChange(lineNumber, orientationDegrees);
                                }
                            },
                            [self](int lineNumber, int sourceVertexIndex, qreal orientationDegrees, qreal leftSize) {
                                if (self != nullptr) {
                                    self->recordLinePointLeftHandleChange(lineNumber, sourceVertexIndex, orientationDegrees, leftSize);
                                }
                            });

    owner_->restoreBackgroundImageItems();
    owner_->reprojectMetadataBackgroundLayersForCurrentDocument();
    owner_->restoreDraftGeometryItems();
    owner_->selectMapLine(owner_->textEditor_->currentLineNumber());
    owner_->applyInspectorObjectVisibility();
    owner_->updateGeometrySelectionPresentation();
    if (owner_->autoFitEnabled_) {
        owner_->fitMapToView(owner_->fitBackgroundRequested_);
    } else {
        owner_->syncZoomFactorFromView();
    }
    owner_->updateInteractiveDrawPreview();
    owner_->refreshStatus();
    owner_->updateCommandSurfaceState();
    owner_->updateHelpPanel();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorSceneRefreshController::flushPendingMapSceneRefreshAfterCommand()
{
    if (!owner_->mapSceneRefreshPending_) {
        return;
    }

    owner_->mapSceneRefreshPending_ = false;
    refreshMapScenePreservingUndoStack();
}

}
