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
#include "MapEditorInputPolicy.h"
#include "MapEditorViewportInputController.h"
#include "MapEditorCanvasEditController.h"
#include "MapEditorSceneLifecycleController.h"
#include "MapEditorSceneRefreshController.h"
#include "MapEditorSelectionController.h"
#include "MapEditorInspectorData.h"
#include "MapEditorInspectorBackgroundController.h"
#include "MapEditorInspectorObjectController.h"
#include "MapEditorInteractiveDrawController.h"
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
constexpr int kMapItemRole = Qt::UserRole + 120;
constexpr int kMapItemGeometryValue = 1;
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;
constexpr int kInspectorSourceFileRole = Qt::UserRole + 701;
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectVisibilityColumn = 1;
constexpr int kInspectorObjectDeleteColumn = 2;
constexpr int kInspectorObjectColumnCount = 3;
constexpr int kInspectorBackgroundNameColumn = 0;
constexpr int kInspectorBackgroundVisibilityColumn = 1;
constexpr int kInspectorBackgroundDeleteColumn = 2;
constexpr int kInspectorBackgroundColumnCount = 3;
constexpr int kInspectorBackgroundLayerIndexRole = Qt::UserRole + 730;

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
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
                const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
                if (index.column() == kInspectorObjectNameColumn
                    && objectIndex.isValid()
                    && mapObjectsModel_ != nullptr
                    && mapObjectsModel_->rowCount(objectIndex) > 0
                    && mouseEvent->pos().x() < mapObjectsTree_->visualRect(objectIndex).left() + mapObjectsTree_->indentation()) {
                    return QWidget::eventFilter(watched, event);
                }
                if (index.isValid()
                    && (index.column() == kInspectorObjectNameColumn
                        || index.column() == kInspectorObjectVisibilityColumn
                        || index.column() == kInspectorObjectDeleteColumn)) {
                    event->accept();
                    return true;
                }
            }
        }

        if (event->type() == QEvent::MouseButtonPress) {
            inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
            inspectorObjectPressedWasSelected_ = false;

            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && mapObjectsTree_->selectionModel() != nullptr) {
                const QModelIndex index = mapObjectsTree_->indexAt(mouseEvent->pos());
                const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
                if (index.column() == kInspectorObjectNameColumn
                    && objectIndex.isValid()
                    && mapObjectsModel_ != nullptr
                    && mapObjectsModel_->rowCount(objectIndex) > 0
                    && mouseEvent->pos().x() < mapObjectsTree_->visualRect(objectIndex).left() + mapObjectsTree_->indentation()) {
                    return QWidget::eventFilter(watched, event);
                }
                if (index.isValid()
                    && (index.column() == kInspectorObjectNameColumn
                        || index.column() == kInspectorObjectVisibilityColumn
                        || index.column() == kInspectorObjectDeleteColumn)) {
                    if (index.column() == kInspectorObjectNameColumn) {
                        inspectorObjectPressedNameIndex_ = QPersistentModelIndex(objectIndex);
                        inspectorObjectPressedWasSelected_ = mapObjectsTree_->selectionModel()->isSelected(objectIndex);
                    }
                    handleInspectorObjectClicked(index);
                    event->accept();
                    return true;
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    if (const std::optional<bool> viewportResult = MapEditorViewportInputController(this).handleEvent(watched, event)) {
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
    MapEditorSceneRefreshController(this).buildMapScene();
}

void MapEditorTab::refreshMapScene()
{
    MapEditorSceneRefreshController(this).refreshMapScene();
}

void MapEditorTab::refreshMapScenePreservingUndoStack()
{
    MapEditorSceneRefreshController(this).refreshMapScenePreservingUndoStack();
}

void MapEditorTab::flushPendingMapSceneRefreshAfterCommand()
{
    MapEditorSceneRefreshController(this).flushPendingMapSceneRefreshAfterCommand();
}

void MapEditorTab::handleMapSceneSelectionChanged()
{
    MapEditorSelectionController(this).handleMapSceneSelectionChanged();
}

void MapEditorTab::syncMapSelectionFromTextCursor(int lineNumber, int columnNumber)
{
    MapEditorSelectionController(this).syncMapSelectionFromTextCursor(lineNumber, columnNumber);
}

void MapEditorTab::updateGeometrySelectionPresentation()
{
    MapEditorSelectionController(this).updateGeometrySelectionPresentation();
}

void MapEditorTab::handleAddPointTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Point);
    toolbarStatusNote_ = tr("Point mode: click in map to insert a point.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddLineTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Line);
    toolbarStatusNote_ = tr("Line mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddFreehandLineTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Freehand);
    toolbarStatusNote_ = tr("Freehand mode: drag in map to draw a line, then release to finish.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddSmartTraceLineTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::None);
    selectModeActive_ = false;
    toolbarStatusNote_ = tr("Smart Trace mode: inserted a trace-ready draft line. Complete Draft commits one undo step.");
    addDraftGeometryItem(createDraftGeometryItem(DraftGeometryKind::Line), mapView_ != nullptr ? mapView_->mapToScene(mapView_->viewport()->rect().center()) : QPointF(190.0, 190.0));
    refreshToolbarSummary();
}

void MapEditorTab::handleAddAreaTriggered()
{
    setInteractiveDrawMode(InteractiveDrawMode::Area);
    toolbarStatusNote_ = tr("Area mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleSelectModeTriggered()
{
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
    recordSourceTextSnapshot(tr("Complete Draft"), beforeText, textEditor_->text(), insertedLineNumber);

    const QString geometryLabel = draftGeometryLabel(draftItem->kind());
    removeDraftGeometryItem(draftRectItem);
    toolbarStatusNote_ = insertedLineNumber > 0
        ? tr("Complete Draft wrote %1 geometry at source line %2.").arg(geometryLabel, QString::number(insertedLineNumber))
        : tr("Complete Draft wrote %1 geometry to source.").arg(geometryLabel);
    refreshToolbarSummary();
    updateHelpPanel();
}

void MapEditorTab::setInteractiveDrawMode(InteractiveDrawMode mode)
{
    MapEditorInteractiveDrawController(this).setInteractiveDrawMode(mode);
}

bool MapEditorTab::handleInteractiveDrawClick(const QPointF &scenePosition)
{
    return MapEditorInteractiveDrawController(this).handleInteractiveDrawClick(scenePosition);
}

bool MapEditorTab::commitInteractiveDrawSession()
{
    return MapEditorInteractiveDrawController(this).commitInteractiveDrawSession();
}

void MapEditorTab::clearInteractiveDrawSession(bool clearMode)
{
    MapEditorInteractiveDrawController(this).clearInteractiveDrawSession(clearMode);
}

void MapEditorTab::updateInteractiveDrawPreview()
{
    MapEditorInteractiveDrawController(this).updateInteractiveDrawPreview();
}

QRectF MapEditorTab::mapSourceBoundsForCurrentDocument() const
{
    if (textEditor_ == nullptr) {
        return QRectF();
    }

    const QString currentText = textEditor_->text();
    const TherionAreaAdjust areaAdjust = parseTherionAreaAdjust(currentText);
    if (areaAdjust.valid && areaAdjust.modelRect.isValid()) {
        return areaAdjust.modelRect;
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(currentText);
    const QVector<MapGeometryFeature> features = collectGeometryFeatures(parsedLines);
    return geometryBoundsForFeatures(features);
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
    if (!textEditor_->insertDraftGeometry(geometryKind, vertices, &insertedLineNumber, &errorMessage)) {
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

bool MapEditorTab::cancelInteractiveDrawingToSelectMode()
{
    return MapEditorInteractiveDrawController(this).cancelInteractiveDrawingToSelectMode();
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
    MapEditorInspectorObjectController(this).rebuildInspectorObjectsTree();
}

void MapEditorTab::configureInspectorObjectTreeColumns()
{
    MapEditorInspectorObjectController(this).configureInspectorObjectTreeColumns();
}

QModelIndex MapEditorTab::findInspectorObjectIndexForLine(int lineNumber) const
{
    return MapEditorInspectorObjectController(this).findInspectorObjectIndexForLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber)
{
    MapEditorInspectorObjectController(this).syncInspectorObjectSelectionToLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection)
{
    MapEditorInspectorObjectController(this).syncInspectorObjectSelectionToLine(lineNumber, scrollToSelection);
}

void MapEditorTab::setInspectorObjectCurrentIndex(const QModelIndex &index)
{
    MapEditorInspectorObjectController(this).setInspectorObjectCurrentIndex(index);
}

void MapEditorTab::clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers)
{
    MapEditorInspectorObjectController(this).clearInspectorObjectSelection(suppressAutoReselectLineNumbers);
}

void MapEditorTab::handleInspectorObjectSelectionChanged(const QModelIndex &current)
{
    MapEditorInspectorObjectController(this).handleInspectorObjectSelectionChanged(current);
}

void MapEditorTab::handleInspectorObjectClicked(const QModelIndex &index)
{
    MapEditorInspectorObjectController(this).handleInspectorObjectClicked(index);
}

void MapEditorTab::applyInspectorObjectVisibility()
{
    MapEditorInspectorObjectController(this).applyInspectorObjectVisibility();
}

void MapEditorTab::configureInspectorBackgroundLayerTreeColumns()
{
    MapEditorInspectorBackgroundController(this).configureInspectorBackgroundLayerTreeColumns();
}

void MapEditorTab::handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current)
{
    MapEditorInspectorBackgroundController(this).handleInspectorBackgroundLayerSelectionChanged(current);
}

void MapEditorTab::handleInspectorBackgroundLayerClicked(const QModelIndex &index)
{
    MapEditorInspectorBackgroundController(this).handleInspectorBackgroundLayerClicked(index);
}

void MapEditorTab::refreshInspectorBackgroundPanel()
{
    MapEditorInspectorBackgroundController(this).refreshInspectorBackgroundPanel();
}

void MapEditorTab::refreshObjectDetailsPanel()
{
    MapEditorObjectDetailsPanelController(this).refreshObjectDetailsPanel();
}

void MapEditorTab::applyObjectOrientationEdits()
{
    MapEditorObjectDetailsEditController(this).applyObjectOrientationEdits();
}

void MapEditorTab::handleObjectOrientationEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(this).handleObjectOrientationEnabledToggled(checked);
}

void MapEditorTab::handleLinePointLeftSizeEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(this).handleLinePointLeftSizeEnabledToggled(checked);
}

void MapEditorTab::deleteSelectedObjectFromSelection()
{
    MapEditorObjectDetailsEditController(this).deleteSelectedObjectFromSelection();
}

void MapEditorTab::applyObjectQuickFieldEdits()
{
    MapEditorObjectDetailsEditController(this).applyObjectQuickFieldEdits();
}

void MapEditorTab::applyScrapProjectionEdit()
{
    MapEditorObjectDetailsEditController(this).applyScrapProjectionEdit();
}

void MapEditorTab::updateObjectQuickSubtypeChoices()
{
    MapEditorObjectDetailsEditController(this).updateObjectQuickSubtypeChoices();
}

void MapEditorTab::insertVertexFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(this).insertVertexFromSelectionPanel();
}

void MapEditorTab::deleteVertexFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(this).deleteVertexFromSelectionPanel();
}

void MapEditorTab::toggleVertexSmoothFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(this).toggleVertexSmoothFromSelectionPanel();
}

void MapEditorTab::populateScrapScaleFromSourceBounds()
{
    MapEditorObjectDetailsEditController(this).populateScrapScaleFromSourceBounds();
}

void MapEditorTab::applyScrapScaleEdits()
{
    MapEditorObjectDetailsEditController(this).applyScrapScaleEdits();
}

void MapEditorTab::handleConfigureObjectSettingsTriggered()
{
    MapEditorObjectDetailsEditController(this).handleConfigureObjectSettingsTriggered();
}

void MapEditorTab::handleLineClosedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(this).handleLineClosedToggled(checked);
}

void MapEditorTab::handleLineReversedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(this).handleLineReversedToggled(checked);
}

void MapEditorTab::clearMapScene()
{
    MapEditorSceneLifecycleController(this).clearMapScene();
}

void MapEditorTab::clearDraftGeometryItems()
{
    MapEditorSceneLifecycleController(this).clearDraftGeometryItems();
}

void MapEditorTab::clearBackgroundImageItems()
{
    MapEditorSceneLifecycleController(this).clearBackgroundImageItems();
}

void MapEditorTab::restoreDraftGeometryItems()
{
    MapEditorSceneLifecycleController(this).restoreDraftGeometryItems();
}

void MapEditorTab::restoreBackgroundImageItems()
{
    MapEditorSceneLifecycleController(this).restoreBackgroundImageItems();
}

void MapEditorTab::fitMapToView(bool includeBackgroundImages)
{
    MapEditorSceneLifecycleController(this).fitMapToView(includeBackgroundImages);
}

void MapEditorTab::syncZoomFactorFromView()
{
    MapEditorSceneLifecycleController(this).syncZoomFactorFromView();
}

void MapEditorTab::applyZoomAtViewportPosition(qreal factor, const QPointF &viewportPosition)
{
    MapEditorSceneLifecycleController(this).applyZoomAtViewportPosition(factor, viewportPosition);
}

void MapEditorTab::refreshToolbarSummary()
{
    // Status text is retained for future command-surface summaries.
}

QRectF MapEditorTab::mapGeometryFitBounds() const
{
    return MapEditorSceneLifecycleController(this).mapGeometryFitBounds();
}

QRectF MapEditorTab::mapPreviewBounds() const
{
    return MapEditorSceneLifecycleController(this).mapPreviewBounds();
}

void MapEditorTab::adjustMapZoom(qreal factor)
{
    MapEditorSceneLifecycleController(this).adjustMapZoom(factor);
}

void MapEditorTab::recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition)
{
    MapEditorCanvasEditController(this).recordCardMove(lineNumber, oldPosition, newPosition);
}

void MapEditorTab::recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible)
{
    MapEditorCanvasEditController(this).recordCardVisibility(lineNumber, oldVisible, newVisible);
}

void MapEditorTab::recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint)
{
    MapEditorCanvasEditController(this).recordPointGeometryMove(lineNumber, oldPoint, newPoint);
}

void MapEditorTab::recordLineAreaVertexMove(int lineNumber,
                                            const QString &kind,
                                            int vertexIndex,
                                            const QPointF &oldPoint,
                                            const QPointF &newPoint)
{
    MapEditorCanvasEditController(this).recordLineAreaVertexMove(lineNumber, kind, vertexIndex, oldPoint, newPoint);
}

void MapEditorTab::recordPointOrientationHandleChange(int lineNumber, qreal orientationDegrees)
{
    MapEditorCanvasEditController(this).recordPointOrientationHandleChange(lineNumber, orientationDegrees);
}

void MapEditorTab::recordLinePointLeftHandleChange(int lineNumber,
                                                   int sourceVertexIndex,
                                                   qreal orientationDegrees,
                                                   qreal leftSize)
{
    MapEditorCanvasEditController(this).recordLinePointLeftHandleChange(lineNumber, sourceVertexIndex, orientationDegrees, leftSize);
}

void MapEditorTab::restorePointSelection(int lineNumber)
{
    MapEditorCanvasEditController(this).restorePointSelection(lineNumber);
}

void MapEditorTab::restoreLineAnchorSelection(int lineNumber, int sourceVertexIndex)
{
    MapEditorCanvasEditController(this).restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
}

void MapEditorTab::recordSourceTextSnapshot(const QString &label,
                                            const QString &beforeText,
                                            const QString &afterText,
                                            int insertedLineNumber)
{
    MapEditorCanvasEditController(this).recordSourceTextSnapshot(label, beforeText, afterText, insertedLineNumber);
}

bool MapEditorTab::insertLineVertexFromSelection()
{
    return MapEditorCanvasEditController(this).insertLineVertexFromSelection();
}

bool MapEditorTab::removeLineVertexFromSelection()
{
    return MapEditorCanvasEditController(this).removeLineVertexFromSelection();
}

bool MapEditorTab::toggleLineVertexSmoothFromSelection()
{
    return MapEditorCanvasEditController(this).toggleLineVertexSmoothFromSelection();
}

QGraphicsRectItem *MapEditorTab::selectedDraftGeometryItem() const
{
    return MapEditorCanvasEditController(this).selectedDraftGeometryItem();
}

QGraphicsRectItem *MapEditorTab::createDraftGeometryItem(DraftGeometryKind kind)
{
    return MapEditorCanvasEditController(this).createDraftGeometryItem(kind);
}

void MapEditorTab::addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position)
{
    MapEditorCanvasEditController(this).addDraftGeometryItem(item, position);
}

void MapEditorTab::removeDraftGeometryItem(QGraphicsRectItem *item)
{
    MapEditorCanvasEditController(this).removeDraftGeometryItem(item);
}

QVector<QPointF> MapEditorTab::sourceVerticesForDraft(const QGraphicsRectItem *item) const
{
    return MapEditorCanvasEditController(this).sourceVerticesForDraft(item);
}

QPointF MapEditorTab::previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const
{
    return MapEditorCanvasEditController(this).previewToSourcePoint(previewPoint, sourceBounds, previewBounds);
}

void MapEditorTab::recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    MapEditorCanvasEditController(this).recordDraftMove(item, oldPosition, newPosition);
}

void MapEditorTab::recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible)
{
    MapEditorCanvasEditController(this).recordDraftVisibility(item, oldVisible, newVisible);
}

void MapEditorTab::selectMapLine(int lineNumber, bool centerOnSelection)
{
    MapEditorSelectionController(this).selectMapLine(lineNumber, centerOnSelection);
}

void MapEditorTab::selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection)
{
    MapEditorSelectionController(this).selectMapLines(lineNumbers, centerOnSelection);
}

}
