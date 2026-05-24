#include "MapEditorTab.h"

#include <QApplication>
#include <QEvent>
#include <QCursor>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsPathItem>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QLabel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QSet>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QShortcut>
#include <QTimer>
#include <QTreeView>
#include <QStandardItemModel>
#include <QPainterPath>

#include <optional>

#include "MapEditorSceneSupport.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSourceReferenceResolver.h"
#include "MapEditorInputPolicy.h"
#include "MapEditorViewportInputController.h"
#include "MapEditorSceneLifecycleController.h"
#include "MapEditorSceneRefreshController.h"
#include "MapEditorInspectorData.h"
#include "MapEditorInspectorBackgroundController.h"
#include "MapEditorInspectorObjectController.h"
#include "MapEditorMagnifierOverlay.h"
#include "MapEditorObjectDetailsEditController.h"
#include "MapEditorObjectDetailsPanelController.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../TextEditorTab.h"
#include "../../../core/TherionBackgroundMetadata.h"
#include "../../../core/TherionDocumentParser.h"

namespace TherionStudio
{
namespace
{
int lineVertexIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        if (lineFeature.lineVertices.at(index).anchorSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

MapGeometryFeature::TH2LineVertex draftVertexToLineVertex(const MapEditorInteractiveLineDraftVertex &draftVertex)
{
    MapGeometryFeature::TH2LineVertex lineVertex;
    lineVertex.anchor = draftVertex.anchorSource;
    lineVertex.incomingControl = draftVertex.incomingControlSource;
    lineVertex.outgoingControl = draftVertex.outgoingControlSource;
    lineVertex.anchorSourceVertexIndex = -1;
    lineVertex.incomingSourceVertexIndex = -1;
    lineVertex.outgoingSourceVertexIndex = -1;
    return lineVertex;
}
}

bool MapEditorTab::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && watched == qApp) {
        switch (event->type()) {
        case QEvent::ApplicationPaletteChange:
        case QEvent::PaletteChange:
        case QEvent::StyleChange:
            handleApplicationAppearanceChanged();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (event == nullptr) {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == mapInspectorTabs_) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::StyleChange:
        case QEvent::PaletteChange:
            updateMapInspectorLeftEdgeGeometry();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (mapObjectsTree_ != nullptr && watched == mapObjectsTree_->viewport()) {
        if (const std::optional<bool> inspectorResult = handleInspectorObjectViewportEvent(event)) {
            return inspectorResult.value();
        }
        return QWidget::eventFilter(watched, event);
    }

    if (mapView_ != nullptr && watched == mapView_->viewport()) {
        switch (event->type()) {
        case QEvent::MouseMove:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
            scheduleMagnifierOverlayUpdateFromViewportPosition(static_cast<QMouseEvent *>(event)->pos());
            break;
        case QEvent::Leave:
            hideMagnifierOverlay();
            break;
        case QEvent::Resize:
        case QEvent::Show:
            updateMagnifierOverlayGeometry();
            break;
        default:
            break;
        }
    }

    if (const std::optional<bool> viewportResult = MapEditorViewportInputController(viewportInputContext()).handleEvent(watched, event)) {
        return viewportResult.value();
    }

    return QWidget::eventFilter(watched, event);
}


void MapEditorTab::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event == nullptr) {
        return;
    }

    switch (event->type()) {
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        handleApplicationAppearanceChanged();
        break;
    default:
        break;
    }
}

void MapEditorTab::handleApplicationAppearanceChanged()
{
    if (mapView_ != nullptr) {
        mapView_->setBackgroundBrush(mapView_->palette().color(QPalette::Window));
        if (QWidget *viewport = mapView_->viewport(); viewport != nullptr) {
            viewport->update();
        }
    }

    if (mapScene_ != nullptr) {
        refreshMapScenePreservingUndoStack();
    }
}

void MapEditorTab::buildMapScene()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).buildMapScene();
}

void MapEditorTab::refreshMapScene()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).refreshMapScene();
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->update();
    }
}

void MapEditorTab::refreshMapScenePreservingUndoStack()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).refreshMapScenePreservingUndoStack();
    if (mapMagnifierOverlay_ != nullptr) {
        mapMagnifierOverlay_->update();
    }
}

void MapEditorTab::flushPendingMapSceneRefreshAfterCommand()
{
    const bool hadPendingRefresh = mapSceneRefreshPending_;
    MapEditorSceneRefreshController(sceneRefreshContext()).flushPendingMapSceneRefreshAfterCommand();
    if (hadPendingRefresh) {
        rebuildInspectorObjectsTree();
    }
}

void MapEditorTab::scheduleSourceDrivenMapRefresh()
{
    if (mapCommandApplyInProgress_) {
        if (sourceDrivenMapRefreshTimer_ != nullptr) {
            sourceDrivenMapRefreshTimer_->stop();
        }
        refreshMapScene();
        return;
    }

    if (sourceDrivenMapRefreshTimer_ == nullptr) {
        applySourceDrivenMapRefresh();
        return;
    }

    sourceDrivenMapRefreshTimer_->start();
}

void MapEditorTab::applySourceDrivenMapRefresh()
{
    if (mapCommandApplyInProgress_) {
        scheduleSourceDrivenMapRefresh();
        return;
    }

    refreshMapScene();
    rebuildInspectorObjectsTree();
}

QVector<TherionParsedLine> MapEditorTab::parsedLinesForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return {};
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedParsedLinesValid_ && cachedParsedLinesRevision_ == currentRevision) {
        return cachedParsedLines_;
    }

    cachedParsedLines_ = TherionDocumentParser::parseText(textEditor_->text());
    cachedParsedLinesRevision_ = currentRevision;
    cachedParsedLinesValid_ = true;
    return cachedParsedLines_;
}

void MapEditorTab::handleAddPointTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Point);
    toolbarStatusNote_ = tr("Point mode: click in map to insert a point.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddLineTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Line);
    toolbarStatusNote_ = tr("Line mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddFreehandLineTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Freehand);
    toolbarStatusNote_ = tr("Freehand mode: drag in map to draw a line, then release to finish.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddAreaTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Area);
    toolbarStatusNote_ = tr("Area mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleSelectModeTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::None);
    selectModeActive_ = true;
    toolbarStatusNote_ = tr("Selection mode: select map cards or draft objects.");
    if (mapView_ != nullptr) {
        mapView_->setDragMode(QGraphicsView::NoDrag);
    }
    refreshToolbarSummary();
}

void MapEditorTab::handleInsertScrapTriggered()
{
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Insert Scrap failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    const QString scrapScaleOption = xtherionDefaultScrapScaleOption(mapSourceBoundsForCurrentDocument());
    if (!textEditor_->insertScrapBlock(QStringLiteral("new-scrap"), &insertedLineNumber, &errorMessage, scrapScaleOption)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Insert Scrap failed.")
            : tr("Insert Scrap failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }
    recordSourceTextSnapshot(tr("Insert Scrap"), beforeText, textEditor_->text(), insertedLineNumber);

    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Inserted scrap block at line %1.").arg(insertedLineNumber)
        : tr("Inserted scrap block.");
    refreshToolbarSummary();
}

void MapEditorTab::handleCompleteDraftTriggered()
{
    if (commitInteractiveDrawSession()) {
        return;
    }

    auto *draftRectItem = selectedDraftGeometryItem();
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(draftRectItem);
    if (draftItem == nullptr || draftItem->isDraftComplete()) {
        return;
    }

    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return;
    }

    const QVector<QPointF> vertices = sourceVerticesForDraft(draftItem);
    if (vertices.isEmpty()) {
        toolbarStatusNote_ = tr("Complete Draft failed: unable to resolve draft geometry coordinates.");
        refreshToolbarSummary();
        return;
    }

    QString geometryKind;
    switch (draftItem->kind()) {
    case DraftGeometryKind::Point:
        geometryKind = QStringLiteral("point");
        break;
    case DraftGeometryKind::Line:
        geometryKind = QStringLiteral("line");
        break;
    case DraftGeometryKind::Area:
        geometryKind = QStringLiteral("area");
        break;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    if (!textEditor_->insertDraftGeometry(geometryKind, vertices, &insertedLineNumber, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return;
    }
    const QString geometryLabel = draftGeometryLabel(draftItem->kind());
    recordDraftCompletion(draftRectItem, tr("Complete Draft"), beforeText, textEditor_->text(), insertedLineNumber);
    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(geometryLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(geometryLabel);
    refreshToolbarSummary();
    updateHelpPanel();
}

QRectF MapEditorTab::mapSourceBoundsForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return QRectF();
    }

    const int currentRevision = textEditor_->documentRevision();
    if (cachedMapSourceBoundsValid_ && cachedMapSourceBoundsRevision_ == currentRevision) {
        return cachedMapSourceBounds_;
    }

    const QString currentText = textEditor_->text();
    QRectF resolvedBounds;
    const TherionAreaAdjust areaAdjust = parseTherionAreaAdjust(currentText);
    if (areaAdjust.valid && areaAdjust.modelRect.isValid()) {
        resolvedBounds = areaAdjust.modelRect;
    } else {
        const QVector<TherionParsedLine> parsedLines = parsedLinesForCurrentDocument();
        const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
        resolvedBounds = geometryBoundsForFeatures(features);
    }

    cachedMapSourceBoundsValid_ = true;
    cachedMapSourceBoundsRevision_ = currentRevision;
    cachedMapSourceBounds_ = resolvedBounds;
    return cachedMapSourceBounds_;
}

QPointF MapEditorTab::sourcePointFromScenePosition(const QPointF &scenePosition) const
{
    if (textEditor_ == nullptr) {
        return scenePosition;
    }

    const QRectF previewBounds = mapPreviewBounds();
    if (!previewBounds.isValid()) {
        return scenePosition;
    }

    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    if (!sourceBounds.isValid() || sourceBounds.width() < 0.001 || sourceBounds.height() < 0.001) {
        return scenePosition;
    }

    return previewToSourcePoint(scenePosition, sourceBounds, previewBounds);
}

bool MapEditorTab::hasCompletableInteractiveDrawSession() const
{
    if (lineExtensionActive_) {
        return interactiveDrawMode_ == InteractiveDrawMode::Line && interactiveDrawLineVertices_.size() >= 2;
    }
    if (interactiveDrawMode_ == InteractiveDrawMode::Line) {
        return interactiveDrawLineVertices_.size() >= 2;
    }
    if (interactiveDrawMode_ == InteractiveDrawMode::Area) {
        return interactiveDrawLineVertices_.size() >= 3;
    }
    return false;
}

QStringList MapEditorTab::lineCoordinateRowsForInteractiveDraft() const
{
    return TherionStudio::lineCoordinateRowsForInteractiveDraft(interactiveDrawLineVertices_);
}

QStringList MapEditorTab::areaCoordinateRowsForInteractiveDraft() const
{
    return TherionStudio::areaCoordinateRowsForInteractiveDraft(interactiveDrawLineVertices_);
}

void MapEditorTab::captureInteractiveLineAnchor(const QPointF &anchorScenePoint,
                                                const std::optional<QPointF> &dragScenePoint)
{
    TherionStudio::captureInteractiveLineAnchor(&interactiveDrawLineVertices_,
                                                anchorScenePoint,
                                                sourcePointFromScenePosition(anchorScenePoint),
                                                dragScenePoint,
                                                [this](const QPointF &scenePoint) {
                                                    return sourcePointFromScenePosition(scenePoint);
                                                });
    updateInteractiveDrawPreview();
}

std::optional<MapEditorInteractiveLineControlHandleRef> MapEditorTab::interactiveLineControlAt(
    const QPointF &scenePosition,
    qreal sceneRadius) const
{
    return TherionStudio::interactiveLineControlAt(interactiveDrawLineVertices_, scenePosition, sceneRadius);
}

bool MapEditorTab::setInteractiveLineControlScenePoint(const MapEditorInteractiveLineControlHandleRef &handle,
                                                       const QPointF &scenePoint)
{
    return TherionStudio::setInteractiveLineControlScenePoint(&interactiveDrawLineVertices_,
                                                              handle,
                                                              scenePoint,
                                                              [this](const QPointF &scenePointToMap) {
                                                                  return sourcePointFromScenePosition(scenePointToMap);
                                                              });
}

bool MapEditorTab::commitInteractiveDrawVertices(const QString &geometryKind,
                                                 const QVector<QPointF> &vertices,
                                                 const QString &successLabel)
{
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Complete Draft failed: no active TH2 text editor.");
        return false;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    const bool inserted = geometryKind.trimmed().compare(QStringLiteral("line"), Qt::CaseInsensitive) == 0
        ? textEditor_->insertDraftLineGeometry(bezierLineCoordinateRowsForFreehandStroke(vertices),
                                               &insertedLineNumber,
                                               &errorMessage)
        : textEditor_->insertDraftGeometry(geometryKind, vertices, &insertedLineNumber, &errorMessage);
    if (!inserted) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Complete Draft failed.")
            : tr("Complete Draft failed: %1").arg(errorMessage);
        return false;
    }
    recordSourceTextSnapshot(tr("Complete Draft"), beforeText, textEditor_->text(), insertedLineNumber);

    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(successLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(successLabel);
    return true;
}

QPointF MapEditorTab::scenePointFromSourcePosition(const QPointF &sourcePosition) const
{
    const QRectF previewBounds = mapPreviewBounds();
    const QRectF sourceBounds = mapSourceBoundsForCurrentDocument();
    if (!previewBounds.isValid()
        || !sourceBounds.isValid()
        || sourceBounds.width() < 0.001
        || sourceBounds.height() < 0.001) {
        return sourcePosition;
    }
    return sceneCoordsSourceToPreview(sourcePosition, sourceBounds, previewBounds);
}

bool MapEditorTab::beginLineExtensionFromSelection(int lineNumber, int sourceVertexIndex, bool prepend)
{
    if (textEditor_ == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return false;
    }

    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(textEditor_->text(), lineNumber);
    if (!lineFeature.has_value() || lineFeature->lineVertices.size() < 2) {
        return false;
    }

    const int vertexIndex = lineVertexIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
    if ((prepend && vertexIndex != 0) || (!prepend && vertexIndex != lineFeature->lineVertices.size() - 1)) {
        return false;
    }

    clearInteractiveDrawSession(false);
    lineExtensionActive_ = true;
    lineExtensionLineNumber_ = lineNumber;
    lineExtensionPrepend_ = prepend;
    interactiveDrawMode_ = InteractiveDrawMode::Line;
    selectModeActive_ = false;

    const MapGeometryFeature::TH2LineVertex &endpoint = lineFeature->lineVertices.at(vertexIndex);
    MapEditorInteractiveLineDraftVertex seed;
    seed.anchorSource = endpoint.anchor;
    seed.anchorScene = scenePointFromSourcePosition(endpoint.anchor);
    interactiveDrawLineVertices_.append(seed);
    updateInteractiveDrawPreview();
    emit modeStatusChanged();
    updateCommandSurfaceState();

    toolbarStatusNote_ = prepend
        ? tr("Extend Before: continue drawing from the first vertex, then press Enter or Complete Draft.")
        : tr("Extend After: continue drawing from the last vertex, then press Enter or Complete Draft.");
    refreshToolbarSummary();
    return true;
}

bool MapEditorTab::commitLineExtensionSession()
{
    if (!lineExtensionActive_) {
        return false;
    }
    if (textEditor_ == nullptr) {
        toolbarStatusNote_ = tr("Extend line failed: no active TH2 text editor.");
        refreshToolbarSummary();
        return true;
    }
    if (interactiveDrawLineVertices_.size() < 2) {
        toolbarStatusNote_ = tr("Extend line needs at least one new vertex before completion.");
        refreshToolbarSummary();
        return true;
    }

    const QString beforeText = textEditor_->text();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(beforeText, lineExtensionLineNumber_);
    if (!lineFeature.has_value() || lineFeature->lineVertices.size() < 2) {
        toolbarStatusNote_ = tr("Extend line failed: line geometry could not be resolved.");
        refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    if (lineExtensionPrepend_) {
        QVector<MapGeometryFeature::TH2LineVertex> prependedVertices;
        prependedVertices.reserve(interactiveDrawLineVertices_.size() - 1);
        for (int index = interactiveDrawLineVertices_.size() - 1; index >= 1; --index) {
            MapGeometryFeature::TH2LineVertex lineVertex = draftVertexToLineVertex(interactiveDrawLineVertices_.at(index));
            lineVertex.incomingControl = interactiveDrawLineVertices_.at(index).outgoingControlSource;
            lineVertex.outgoingControl = interactiveDrawLineVertices_.at(index).incomingControlSource;
            prependedVertices.append(lineVertex);
        }
        if (interactiveDrawLineVertices_.first().outgoingControlSource.has_value()) {
            editedVertices[0].incomingControl = interactiveDrawLineVertices_.first().outgoingControlSource;
        }
        editedVertices = prependedVertices + editedVertices;
    } else {
        if (interactiveDrawLineVertices_.first().outgoingControlSource.has_value()) {
            editedVertices.last().outgoingControl = interactiveDrawLineVertices_.first().outgoingControlSource;
        }
        for (int index = 1; index < interactiveDrawLineVertices_.size(); ++index) {
            editedVertices.append(draftVertexToLineVertex(interactiveDrawLineVertices_.at(index)));
        }
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!textEditor_->rewriteLineCoordinateRows(lineExtensionLineNumber_, coordinateRows, &errorMessage)) {
        toolbarStatusNote_ = errorMessage.isEmpty()
            ? tr("Extend line failed.")
            : tr("Extend line failed: %1").arg(errorMessage);
        refreshToolbarSummary();
        return true;
    }

    const int extendedLineNumber = lineExtensionLineNumber_;
    const int restoredVertexIndex = lineExtensionPrepend_ ? 0 : editedVertices.size() - 1;
    recordSourceTextSnapshot(tr("Extend Line"), beforeText, textEditor_->text(), extendedLineNumber);
    const std::optional<MapGeometryFeature> refreshedFeature = lineFeatureForLineNumber(textEditor_->text(), extendedLineNumber);
    const int restoredSourceVertexIndex =
        refreshedFeature.has_value() && restoredVertexIndex >= 0 && restoredVertexIndex < refreshedFeature->lineVertices.size()
        ? refreshedFeature->lineVertices.at(restoredVertexIndex).anchorSourceVertexIndex
        : -1;
    lineExtensionActive_ = false;
    lineExtensionLineNumber_ = 0;
    lineExtensionPrepend_ = false;
    clearInteractiveDrawSession(true);
    if (restoredSourceVertexIndex >= 0) {
        restoreLineAnchorSelection(extendedLineNumber, restoredSourceVertexIndex);
        QTimer::singleShot(0, this, [this, extendedLineNumber, restoredSourceVertexIndex]() {
            restoreLineAnchorSelection(extendedLineNumber, restoredSourceVertexIndex);
        });
    }
    toolbarStatusNote_ = tr("Extended line on source line %1.").arg(extendedLineNumber);
    refreshToolbarSummary();
    updateHelpPanel();
    updateCommandSurfaceState();
    return true;
}

}
