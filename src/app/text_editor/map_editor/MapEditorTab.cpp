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
#include "MapEditorSceneLifecycleController.h"
#include "MapEditorSceneRefreshController.h"
#include "MapEditorInspectorData.h"
#include "MapEditorInspectorBackgroundController.h"
#include "MapEditorInspectorObjectController.h"
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
}

void MapEditorTab::refreshMapScenePreservingUndoStack()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).refreshMapScenePreservingUndoStack();
}

void MapEditorTab::flushPendingMapSceneRefreshAfterCommand()
{
    MapEditorSceneRefreshController(sceneRefreshContext()).flushPendingMapSceneRefreshAfterCommand();
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
    MapEditorInspectorObjectController(inspectorObjectContext()).rebuildInspectorObjectsTree();
}

void MapEditorTab::configureInspectorObjectTreeColumns()
{
    MapEditorInspectorObjectController(inspectorObjectContext()).configureInspectorObjectTreeColumns();
}

QModelIndex MapEditorTab::findInspectorObjectIndexForLine(int lineNumber) const
{
    return MapEditorInspectorObjectController(const_cast<MapEditorTab *>(this)->inspectorObjectContext()).findInspectorObjectIndexForLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).syncInspectorObjectSelectionToLine(lineNumber);
}

void MapEditorTab::syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).syncInspectorObjectSelectionToLine(lineNumber, scrollToSelection);
}

void MapEditorTab::setInspectorObjectCurrentIndex(const QModelIndex &index)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).setInspectorObjectCurrentIndex(index);
}

void MapEditorTab::clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).clearInspectorObjectSelection(suppressAutoReselectLineNumbers);
}

void MapEditorTab::handleInspectorObjectSelectionChanged(const QModelIndex &current)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).handleInspectorObjectSelectionChanged(current);
}

void MapEditorTab::handleInspectorObjectClicked(const QModelIndex &index)
{
    MapEditorInspectorObjectController(inspectorObjectContext()).handleInspectorObjectClicked(index);
}

void MapEditorTab::applyInspectorObjectVisibility()
{
    MapEditorInspectorObjectController(inspectorObjectContext()).applyInspectorObjectVisibility();
}

void MapEditorTab::configureInspectorBackgroundLayerTreeColumns()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).configureInspectorBackgroundLayerTreeColumns();
}

void MapEditorTab::handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current)
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).handleInspectorBackgroundLayerSelectionChanged(current);
}

void MapEditorTab::handleInspectorBackgroundLayerClicked(const QModelIndex &index)
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).handleInspectorBackgroundLayerClicked(index);
}

void MapEditorTab::refreshInspectorBackgroundPanel()
{
    MapEditorInspectorBackgroundController(inspectorBackgroundContext()).refreshInspectorBackgroundPanel();
}

void MapEditorTab::refreshObjectDetailsPanel()
{
    MapEditorObjectDetailsPanelController(objectDetailsContext()).refreshObjectDetailsPanel();
}

void MapEditorTab::applyObjectOrientationEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectOrientationEdits();
}

void MapEditorTab::handleObjectOrientationEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleObjectOrientationEnabledToggled(checked);
}

void MapEditorTab::handleLinePointLeftSizeEnabledToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLinePointLeftSizeEnabledToggled(checked);
}

void MapEditorTab::deleteSelectedObjectFromSelection()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).deleteSelectedObjectFromSelection();
}

void MapEditorTab::applyObjectQuickFieldEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyObjectQuickFieldEdits();
}

void MapEditorTab::applyScrapProjectionEdit()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyScrapProjectionEdit();
}

void MapEditorTab::updateObjectQuickSubtypeChoices()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).updateObjectQuickSubtypeChoices();
}

void MapEditorTab::insertVertexFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).insertVertexFromSelectionPanel();
}

void MapEditorTab::deleteVertexFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).deleteVertexFromSelectionPanel();
}

void MapEditorTab::toggleVertexSmoothFromSelectionPanel()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).toggleVertexSmoothFromSelectionPanel();
}

void MapEditorTab::populateScrapScaleFromSourceBounds()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).populateScrapScaleFromSourceBounds();
}

void MapEditorTab::applyScrapScaleEdits()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).applyScrapScaleEdits();
}

void MapEditorTab::handleConfigureObjectSettingsTriggered()
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleConfigureObjectSettingsTriggered();
}

void MapEditorTab::handleLineClosedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLineClosedToggled(checked);
}

void MapEditorTab::handleLineReversedToggled(bool checked)
{
    MapEditorObjectDetailsEditController(objectDetailsContext()).handleLineReversedToggled(checked);
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
}

void MapEditorTab::refreshToolbarSummary()
{
    // Status text is retained for future command-surface summaries.
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

}
