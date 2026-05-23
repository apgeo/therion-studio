#include "MapEditorCanvasEditController.h"

#include "../TextEditorTab.h"
#include "MapEditorCanvasEditCommandFactory.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentEditor.h"

#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QScopedValueRollback>
#include <QTimer>
#include <QObject>
#include <QUndoCommand>
#include <QUndoStack>

#include <memory>
#include <optional>
#include <utility>

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

int lineVertexOwnerIndexForSourceVertex(const MapGeometryFeature &lineFeature, int sourceVertexIndex)
{
    if (sourceVertexIndex < 0) {
        return -1;
    }

    for (int index = 0; index < lineFeature.lineVertices.size(); ++index) {
        const MapGeometryFeature::TH2LineVertex &vertex = lineFeature.lineVertices.at(index);
        if (vertex.anchorSourceVertexIndex == sourceVertexIndex
            || vertex.incomingSourceVertexIndex == sourceVertexIndex
            || vertex.outgoingSourceVertexIndex == sourceVertexIndex) {
            return index;
        }
    }

    return -1;
}

MapEditableGeometryVertexItem *findGeometryVertexItem(QGraphicsScene *scene,
                                                      int lineNumber,
                                                      int sourceVertexIndex,
                                                      const QString &geometryKindPrefix)
{
    if (scene == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return nullptr;
    }

    const auto items = scene->items();
    for (QGraphicsItem *item : items) {
        auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
        if (vertexItem == nullptr) {
            continue;
        }
        if (vertexItem->lineNumber() != lineNumber || vertexItem->vertexIndex() != sourceVertexIndex) {
            continue;
        }
        if (!geometryKindPrefix.isEmpty() && !vertexItem->geometryKind().startsWith(geometryKindPrefix)) {
            continue;
        }
        return vertexItem;
    }

    return nullptr;
}

}

QString MapEditorCanvasEditController::tr(const char *text) const
{
    return context_.translate ? context_.translate(text) : QString::fromUtf8(text);
}

void MapEditorCanvasEditController::resetPendingClickSelection()
{
    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();
}

MapEditorCanvasEditController::MapEditorCanvasEditController(MapEditorCanvasEditContext context)
    : context_(std::move(context))
{
}

void MapEditorCanvasEditController::recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition)
{
    if (context_.undoStack == nullptr || oldPosition == newPosition) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>((*context_.itemsByLine).value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    context_.undoStack->push(createMapCardMoveCommand(card, oldPosition, newPosition));
}

void MapEditorCanvasEditController::recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible)
{
    if (context_.undoStack == nullptr || oldVisible == newVisible) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>((*context_.itemsByLine).value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    context_.undoStack->push(createMapCardVisibilityCommand(card, oldVisible, newVisible));
}

void MapEditorCanvasEditController::recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint)
{
    if (lineNumber <= 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || context_.textEditor == nullptr) {
        return;
    }

    auto statusCallback = [statusNote = context_.toolbarStatusNote,
                           refreshToolbarSummary = context_.refreshToolbarSummary](const QString &statusMessage) {
        *statusNote = statusMessage;
        refreshToolbarSummary();
    };

    if (context_.undoStack != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        context_.undoStack->push(createMapPointGeometryMoveCommand(context_.textEditor,
                                                       lineNumber,
                                                       oldPoint,
                                                       newPoint,
                                                       statusCallback));
        context_.flushPendingSceneRefreshAfterCommand();
        return;
    }

    const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
    std::unique_ptr<QUndoCommand> directCommand(createMapPointGeometryMoveCommand(context_.textEditor,
                                                                             lineNumber,
                                                                             oldPoint,
                                                                             newPoint,
                                                                             statusCallback));
    directCommand->redo();
    context_.flushPendingSceneRefreshAfterCommand();
}

void MapEditorCanvasEditController::recordLineAreaVertexMove(int lineNumber,
                                            const QString &kind,
                                            int vertexIndex,
                                            const QPointF &oldPoint,
                                            const QPointF &newPoint)
{
    if (lineNumber <= 0 || vertexIndex < 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || context_.textEditor == nullptr) {
        return;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    const QString rewriteKind = normalizedKind.startsWith(QStringLiteral("line"))
        ? QStringLiteral("line")
        : (normalizedKind.startsWith(QStringLiteral("area")) ? QStringLiteral("area") : normalizedKind);
    if (rewriteKind != QStringLiteral("line") && rewriteKind != QStringLiteral("area")) {
        (*context_.toolbarStatusNote) = tr("Geometry move failed: unsupported geometry kind '%1'.").arg(kind);
        context_.refreshToolbarSummary();
        return;
    }

    QVector<MapLineAreaVertexSecondaryMove> secondaryMoves;
    if (rewriteKind == QStringLiteral("line")) {
        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), lineNumber);
        if (lineFeature.has_value()) {
            secondaryMoves = coupledLineVertexMoveSet(lineFeature.value(), vertexIndex, oldPoint, newPoint).secondaryMoves;
            for (auto it = secondaryMoves.begin(); it != secondaryMoves.end();) {
                if (it->vertexIndex == vertexIndex) {
                    it = secondaryMoves.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    auto statusCallback = [statusNote = context_.toolbarStatusNote,
                           refreshToolbarSummary = context_.refreshToolbarSummary](const QString &statusMessage) {
        *statusNote = statusMessage;
        refreshToolbarSummary();
    };

    if (context_.undoStack != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        context_.undoStack->push(createMapLineAreaVertexMoveCommand(context_.textEditor,
                                                           lineNumber,
                                                           rewriteKind,
                                                           vertexIndex,
                                                           oldPoint,
                                                           newPoint,
                                                           secondaryMoves,
                                                           statusCallback));
        context_.flushPendingSceneRefreshAfterCommand();
        return;
    }

    const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
    std::unique_ptr<QUndoCommand> directCommand(createMapLineAreaVertexMoveCommand(context_.textEditor,
                                                                             lineNumber,
                                                                             rewriteKind,
                                                                             vertexIndex,
                                                                             oldPoint,
                                                                             newPoint,
                                                                             secondaryMoves,
                                                                             statusCallback));
    directCommand->redo();
    context_.flushPendingSceneRefreshAfterCommand();
}

void MapEditorCanvasEditController::recordPointOrientationHandleChange(int lineNumber, qreal orientationDegrees)
{
    if (lineNumber <= 0 || context_.textEditor == nullptr) {
        return;
    }

    resetPendingClickSelection();

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewritePointOrientation(&afterText,
                                                        lineNumber,
                                                        true,
                                                        orientationDegrees,
                                                        &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Point orientation update failed.")
            : tr("Point orientation update failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    context_.textEditor->replaceTextForCommand(afterText);
    recordSourceTextSnapshot(tr("Edit Point Orientation"), beforeText, afterText, lineNumber);
    restorePointSelection(lineNumber);
    QTimer::singleShot(0, context_.callbackContext, [restorePointSelectionLater = context_.restorePointSelectionLater, lineNumber]() {
        restorePointSelectionLater(lineNumber);
    });
    (*context_.toolbarStatusNote) = tr("Updated point orientation to %1 degrees.")
        .arg(QString::number(normalizeOrientationDegreesForMapDetails(orientationDegrees), 'f', 1));
    context_.refreshToolbarSummary();
}

void MapEditorCanvasEditController::recordLinePointLeftHandleChange(int lineNumber,
                                                   int sourceVertexIndex,
                                                   qreal orientationDegrees,
                                                   qreal leftSize)
{
    if (lineNumber <= 0 || sourceVertexIndex < 0 || context_.textEditor == nullptr) {
        return;
    }

    resetPendingClickSelection();

    const QString beforeText = context_.textEditor->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteLinePointOrientation(&afterText,
                                                            lineNumber,
                                                            sourceVertexIndex,
                                                            true,
                                                            orientationDegrees,
                                                            &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Line point orientation update failed.")
            : tr("Line point orientation update failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (!TherionDocumentEditor::rewriteLinePointLeftSize(&afterText,
                                                         lineNumber,
                                                         sourceVertexIndex,
                                                         true,
                                                         leftSize,
                                                         &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Line point l-size update failed.")
            : tr("Line point l-size update failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    context_.textEditor->replaceTextForCommand(afterText);
    recordSourceTextSnapshot(tr("Edit Line Point Options"), beforeText, afterText, lineNumber);
    restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
    QTimer::singleShot(0,
                       context_.callbackContext,
                       [restoreLineAnchorSelectionLater = context_.restoreLineAnchorSelectionLater,
                        lineNumber,
                        sourceVertexIndex]() {
        restoreLineAnchorSelectionLater(lineNumber, sourceVertexIndex);
    });
    (*context_.toolbarStatusNote) = tr("Updated line point orientation %1 deg and l-size %2.")
        .arg(QString::number(orientationDegrees, 'f', 1),
             QString::number(leftSize, 'f', 1));
    context_.refreshToolbarSummary();
}

void MapEditorCanvasEditController::restorePointSelection(int lineNumber)
{
    if (context_.scene == nullptr || lineNumber <= 0) {
        return;
    }

    auto selectedItemIt = (*context_.itemsByLine).find(lineNumber);
    if (selectedItemIt == (*context_.itemsByLine).end()) {
        return;
    }

    auto *pointItem = dynamic_cast<MapEditablePointItem *>(selectedItemIt.value());
    if (pointItem == nullptr) {
        return;
    }

    {
        const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
        context_.scene->clearSelection();
        pointItem->setVisible(true);
        pointItem->setSelected(true);
    }

    (*context_.selectedObjectLineNumber) = lineNumber;
    (*context_.selectedObjectVertexIndex) = -1;
    (*context_.selectedObjectKind) = QStringLiteral("point");
    (*context_.selectedObjectCoordinate) = pointItem->sourcePoint();
    context_.updateGeometrySelectionPresentation();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

void MapEditorCanvasEditController::restoreLineAnchorSelection(int lineNumber, int sourceVertexIndex)
{
    if (context_.scene == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return;
    }

    MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(context_.scene,
                                                                        lineNumber,
                                                                        sourceVertexIndex,
                                                                        QStringLiteral("line"));
    if (ownerAnchor == nullptr) {
        return;
    }

    {
        const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
        context_.scene->clearSelection();
        ownerAnchor->setVisible(true);
        ownerAnchor->setSelected(true);
    }

    (*context_.selectedObjectLineNumber) = lineNumber;
    (*context_.selectedObjectVertexIndex) = sourceVertexIndex;
    (*context_.selectedObjectKind) = QStringLiteral("line");
    (*context_.selectedObjectCoordinate) = context_.sourcePointFromScenePosition(ownerAnchor->pos());
    context_.updateGeometrySelectionPresentation();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

void MapEditorCanvasEditController::recordSourceTextSnapshot(const QString &label,
                                            const QString &beforeText,
                                            const QString &afterText,
                                            int insertedLineNumber)
{
    if (context_.textEditor == nullptr || beforeText == afterText) {
        return;
    }

    auto statusCallback = [statusNote = context_.toolbarStatusNote,
                           refreshToolbarSummary = context_.refreshToolbarSummary](const QString &statusMessage) {
        *statusNote = statusMessage;
        refreshToolbarSummary();
    };

    if (context_.undoStack != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        context_.undoStack->push(createMapSourceTextSnapshotCommand(context_.textEditor,
                                                        label,
                                                        beforeText,
                                                        afterText,
                                                        insertedLineNumber,
                                                        statusCallback));
        context_.flushPendingSceneRefreshAfterCommand();
    }
}

bool MapEditorCanvasEditController::insertLineVertexFromSelection()
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (anchorIndex < 0) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: selected line anchor could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    int segmentStartIndex = -1;
    if (anchorIndex < lineFeature->lineVertices.size() - 1) {
        segmentStartIndex = anchorIndex;
    } else if (anchorIndex > 0) {
        segmentStartIndex = anchorIndex - 1;
    }
    if (segmentStartIndex < 0) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: line needs at least one editable segment.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    int insertedIndex = -1;
    if (!insertLineVertexByDeCasteljau(&editedVertices, segmentStartIndex, 0.5, &insertedIndex)) {
        (*context_.toolbarStatusNote) = tr("Insert vertex failed: segment split could not be computed.");
        context_.refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!context_.textEditor->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Insert vertex failed.")
            : tr("Insert vertex failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }

    (*context_.toolbarStatusNote) = insertedIndex >= 0
        ? tr("Inserted line vertex %1 on source line %2.").arg(insertedIndex + 1).arg(lineNumber)
        : tr("Inserted line vertex on source line %1.").arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::removeLineVertexFromSelection()
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Delete vertex failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (anchorIndex < 0) {
        (*context_.toolbarStatusNote) = tr("Delete vertex failed: selected line anchor could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    if (!removeLineVertexWithReconnect(&editedVertices, anchorIndex)) {
        (*context_.toolbarStatusNote) = tr("Delete vertex failed: only middle anchors can be removed while keeping a valid line.");
        context_.refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!context_.textEditor->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Delete vertex failed.")
            : tr("Delete vertex failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }

    (*context_.toolbarStatusNote) = tr("Removed line vertex %1 on source line %2.").arg(anchorIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::toggleLineVertexSmoothFromSelection()
{
    if (context_.scene == nullptr || context_.textEditor == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(), lineNumber);
    if (!lineFeature.has_value()) {
        (*context_.toolbarStatusNote) = tr("Toggle smooth failed: line geometry could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    const int ownerIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (ownerIndex < 0 || ownerIndex >= lineFeature->lineVertices.size()) {
        (*context_.toolbarStatusNote) = tr("Toggle smooth failed: selected line vertex could not be resolved.");
        context_.refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    MapGeometryFeature::TH2LineVertex &target = editedVertices[ownerIndex];
    target.isSmooth = !target.isSmooth;
    if (target.isSmooth) {
        if (target.incomingControl.has_value() && !target.outgoingControl.has_value()) {
            const QPointF vector = target.incomingControl.value() - target.anchor;
            target.outgoingControl = target.anchor - vector;
        } else if (target.outgoingControl.has_value() && !target.incomingControl.has_value()) {
            const QPointF vector = target.outgoingControl.value() - target.anchor;
            target.incomingControl = target.anchor - vector;
        }
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!context_.textEditor->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        (*context_.toolbarStatusNote) = errorMessage.isEmpty()
            ? tr("Toggle smooth failed.")
            : tr("Toggle smooth failed: %1").arg(errorMessage);
        context_.refreshToolbarSummary();
        return true;
    }

    (*context_.toolbarStatusNote) = target.isSmooth
        ? tr("Line vertex %1 on source line %2 set to smooth.").arg(ownerIndex + 1).arg(lineNumber)
        : tr("Line vertex %1 on source line %2 set to corner (smooth off).").arg(ownerIndex + 1).arg(lineNumber);
    context_.refreshToolbarSummary();
    return true;
}

QGraphicsRectItem *MapEditorCanvasEditController::selectedDraftGeometryItem() const
{
    if (context_.scene == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    for (QGraphicsItem *item : selectedItems) {
        if (auto *draftItem = dynamic_cast<QGraphicsRectItem *>(item)) {
            if (dynamic_cast<MapDraftGeometryItem *>(draftItem) != nullptr) {
                return draftItem;
            }
        }
    }

    return nullptr;
}

QGraphicsRectItem *MapEditorCanvasEditController::createDraftGeometryItem(DraftGeometryKind kind)
{
    return new MapDraftGeometryItem((*context_.nextDraftGeometryId)++, kind);
}

void MapEditorCanvasEditController::addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    draftItem->setPos(position);
    draftItem->setMoveCommittedCallback([recordDraftMove = context_.recordDraftMove,
                                         draftItem](int, const QPointF &oldPosition, const QPointF &newPosition) {
        recordDraftMove(draftItem, oldPosition, newPosition);
    });
    draftItem->setVisibilityCommittedCallback([recordDraftVisibility = context_.recordDraftVisibility,
                                               draftItem](int, bool oldVisible, bool newVisible) {
        recordDraftVisibility(draftItem, oldVisible, newVisible);
    });
    if (context_.undoStack != nullptr) {
        context_.undoStack->push(createMapDraftAddCommand(context_.scene, context_.draftGeometryItems, draftItem, position));
    } else {
        if (context_.scene != nullptr) {
            context_.scene->addItem(draftItem);
        }
        (*context_.draftGeometryItems).append(draftItem);
    }

    context_.updateCommandSurfaceState();
}

void MapEditorCanvasEditController::removeDraftGeometryItem(QGraphicsRectItem *item)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    (*context_.draftGeometryItems).removeAll(draftItem);
    if (context_.scene != nullptr) {
        context_.scene->removeItem(draftItem);
    }
    delete draftItem;
}

QVector<QPointF> MapEditorCanvasEditController::sourceVerticesForDraft(const QGraphicsRectItem *item) const
{
    auto *draftItem = dynamic_cast<const MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return {};
    }

    const QRectF itemRect = draftItem->rect();
    const QPointF itemPosition = draftItem->pos();
    const QPointF itemCenter = itemPosition + itemRect.center();
    QVector<QPointF> previewVertices;

    switch (draftItem->kind()) {
    case DraftGeometryKind::Point:
        previewVertices.append(itemCenter);
        break;
    case DraftGeometryKind::Line:
        previewVertices.append(itemPosition + QPointF(itemRect.left() + 16.0, itemRect.center().y()));
        previewVertices.append(itemPosition + QPointF(itemRect.right() - 16.0, itemRect.center().y()));
        break;
    case DraftGeometryKind::Area:
        previewVertices.append(itemPosition + QPointF(itemRect.center().x(), itemRect.top() + 16.0));
        previewVertices.append(itemPosition + QPointF(itemRect.right() - 16.0, itemRect.bottom() - 16.0));
        previewVertices.append(itemPosition + QPointF(itemRect.left() + 16.0, itemRect.bottom() - 16.0));
        break;
    }

    if (previewVertices.isEmpty()) {
        return {};
    }

    const QRectF previewBounds = context_.mapPreviewBounds();
    const QRectF sourceBounds = context_.mapSourceBoundsForCurrentDocument();
    if (!previewBounds.isValid() || !sourceBounds.isValid() || sourceBounds.width() < 0.001 || sourceBounds.height() < 0.001) {
        return previewVertices;
    }

    QVector<QPointF> sourceVertices;
    sourceVertices.reserve(previewVertices.size());
    for (const QPointF &previewVertex : std::as_const(previewVertices)) {
        sourceVertices.append(previewToSourcePoint(previewVertex, sourceBounds, previewBounds));
    }

    return sourceVertices;
}

QPointF MapEditorCanvasEditController::previewToSourcePoint(const QPointF &previewPoint, const QRectF &sourceBounds, const QRectF &previewBounds) const
{
    if (!sourceBounds.isValid() || !previewBounds.isValid() || previewBounds.width() < 0.001 || previewBounds.height() < 0.001) {
        return previewPoint;
    }

    // Use the shared inverse transform without clamping so drafted points
    // outside the fitted geometry strip keep their relative shape instead of
    // collapsing onto one boundary axis.
    return mapGeometryPreviewToSource(previewPoint, sourceBounds, previewBounds);
}

void MapEditorCanvasEditController::recordDraftMove(QGraphicsRectItem *item, const QPointF &oldPosition, const QPointF &newPosition)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || context_.undoStack == nullptr || oldPosition == newPosition) {
        return;
    }

    context_.undoStack->push(createMapDraftMoveCommand(draftItem, oldPosition, newPosition));
}

void MapEditorCanvasEditController::recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || context_.undoStack == nullptr || oldVisible == newVisible) {
        return;
    }

    context_.undoStack->push(createMapDraftVisibilityCommand(draftItem, oldVisible, newVisible));
}

}
