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
#include <QMainWindow>
#include <QMouseEvent>
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
#include "MapEditorInspectorData.h"
#include "MapEditorInspectorBackgroundController.h"
#include "MapEditorInspectorObjectController.h"
#include "MapEditorObjectDetailsEditController.h"
#include "MapEditorObjectDetailsPanelController.h"
#include "../TextEditorTab.h"
#include "../../../core/TherionBackgroundMetadata.h"
#include "../../../core/TherionDocumentParser.h"

namespace TherionStudio
{

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

    if (handleMapEditorEscapeKeyEvent(watched, event)) {
        return true;
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

bool MapEditorTab::isMapEditorEventReceiver(QObject *receiver) const
{
    if (receiver == nullptr) {
        return false;
    }

    if (auto *widget = qobject_cast<QWidget *>(receiver)) {
        if (widget == this || isAncestorOf(widget)) {
            return true;
        }
        if (isVisible() && window() != nullptr && widget->window() == window()) {
            return true;
        }
        if (detachedMapPaneWindow_ != nullptr
            && (widget == detachedMapPaneWindow_.data()
                || detachedMapPaneWindow_->isAncestorOf(widget))) {
            return true;
        }
    }

    for (QObject *object = receiver; object != nullptr; object = object->parent()) {
        if (object == this
            || object == mapPaneContainer_
            || object == mapView_
            || object == objectDetailsPanel_
            || object == mapInspectorTabs_) {
            return true;
        }
        if (detachedMapPaneWindow_ != nullptr && object == detachedMapPaneWindow_.data()) {
            return true;
        }
    }

    return false;
}

bool MapEditorTab::handleMapEditorEscapeKeyEvent(QObject *receiver, QEvent *event)
{
    if (event == nullptr || event->type() != QEvent::KeyPress) {
        return false;
    }
    if (interactiveDrawMode_ == InteractiveDrawMode::None && !lineExtensionActive_) {
        return false;
    }

    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() != Qt::Key_Escape || keyEvent->modifiers() != Qt::NoModifier) {
        return false;
    }
    if (!isMapEditorEventReceiver(receiver)) {
        return false;
    }

    if (!cancelInteractiveDrawingToSelectMode()) {
        return false;
    }

    keyEvent->accept();
    return true;
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
    beginPendingInsertObject(QStringLiteral("point"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Point mode: click in map to insert a point.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddLineTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Line);
    beginPendingInsertObject(QStringLiteral("line"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Line mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddFreehandLineTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Freehand);
    beginPendingInsertObject(QStringLiteral("line"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Freehand mode: drag in map to draw a line, then release to finish.");
    refreshToolbarSummary();
}

void MapEditorTab::handleAddAreaTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::Area);
    beginPendingInsertObject(QStringLiteral("area"));
    activateSelectionInspector();
    refreshObjectDetailsPanel();
    toolbarStatusNote_ = tr("Area mode: click to add vertices, then press Enter or Complete Draft.");
    refreshToolbarSummary();
}

void MapEditorTab::handleSelectModeTriggered()
{
    lineExtensionActive_ = false;
    setInteractiveDrawMode(InteractiveDrawMode::None);
    clearPendingInsertObject();
    refreshObjectDetailsPanel();
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

    lineExtensionActive_ = false;
    if (interactiveDrawMode_ != InteractiveDrawMode::None) {
        setInteractiveDrawMode(InteractiveDrawMode::None);
    }
    selectModeActive_ = true;

    if (pendingInsertFields_.commandKind.trimmed().toLower() != QStringLiteral("scrap")) {
        beginPendingInsertObject(QStringLiteral("scrap"));
        activateSelectionInspector();
        refreshObjectDetailsPanel();
        toolbarStatusNote_ = tr("Scrap insert: set ID/projection in Selection, then click Insert Scrap again.");
        refreshToolbarSummary();
        return;
    }

    QString errorMessage;
    int insertedLineNumber = 0;
    const QString beforeText = textEditor_->text();
    const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
    const QString scrapScaleOption = xtherionDefaultScrapScaleOption(mapSourceBoundsForCurrentDocument());
    if (!textEditor_->insertScrapBlock(pendingScrapPreferredName(),
                                       &insertedLineNumber,
                                       &errorMessage,
                                       pendingScrapOptions(scrapScaleOption))) {
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
    clearPendingInsertObject();
    refreshObjectDetailsPanel();
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
    if (!textEditor_->insertDraftGeometry(geometryKind,
                                          vertices,
                                          &insertedLineNumber,
                                          &errorMessage,
                                          pendingDraftObjectOptions(geometryKind))) {
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
                                               &errorMessage,
                                               QString(),
                                               pendingDraftObjectOptions(QStringLiteral("line")))
        : textEditor_->insertDraftGeometry(geometryKind,
                                           vertices,
                                           &insertedLineNumber,
                                           &errorMessage,
                                           pendingDraftObjectOptions(geometryKind));
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

}
