#include "MapEditorCanvasEditController.h"

#include "../TextEditorTab.h"
#include "MapEditorCanvasEditCommandFactory.h"
#include "MapEditorObjectDetailsLogic.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "MapEditorTab.h"
#include "../../../core/TherionDocumentEditor.h"

#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QScopedValueRollback>
#include <QTimer>
#include <QUndoCommand>
#include <QUndoStack>

#include <memory>
#include <optional>

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

MapEditorCanvasEditController::MapEditorCanvasEditController(MapEditorTab *owner)
    : owner_(owner)
{
}

MapEditorCanvasEditController::MapEditorCanvasEditController(const MapEditorTab *owner)
    : owner_(const_cast<MapEditorTab *>(owner))
{
}

void MapEditorCanvasEditController::recordCardMove(int lineNumber, const QPointF &oldPosition, const QPointF &newPosition)
{
    if (owner_->undoStack_ == nullptr || oldPosition == newPosition) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(owner_->mapItemsByLine_.value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    owner_->undoStack_->push(createMapCardMoveCommand(card, oldPosition, newPosition));
}

void MapEditorCanvasEditController::recordCardVisibility(int lineNumber, bool oldVisible, bool newVisible)
{
    if (owner_->undoStack_ == nullptr || oldVisible == newVisible) {
        return;
    }

    auto *card = dynamic_cast<MapCardItem *>(owner_->mapItemsByLine_.value(lineNumber, nullptr));
    if (card == nullptr) {
        return;
    }

    owner_->undoStack_->push(createMapCardVisibilityCommand(card, oldVisible, newVisible));
}

void MapEditorCanvasEditController::recordPointGeometryMove(int lineNumber, const QPointF &oldPoint, const QPointF &newPoint)
{
    if (lineNumber <= 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || owner_->textEditor_ == nullptr) {
        return;
    }

    auto statusCallback = [owner = owner_](const QString &statusMessage) {
        owner->toolbarStatusNote_ = statusMessage;
        owner->refreshToolbarSummary();
    };

    if (owner_->undoStack_ != nullptr) {
        const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
        owner_->undoStack_->push(createMapPointGeometryMoveCommand(owner_->textEditor_,
                                                       lineNumber,
                                                       oldPoint,
                                                       newPoint,
                                                       statusCallback));
        owner_->flushPendingMapSceneRefreshAfterCommand();
        return;
    }

    const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
    std::unique_ptr<QUndoCommand> directCommand(createMapPointGeometryMoveCommand(owner_->textEditor_,
                                                                             lineNumber,
                                                                             oldPoint,
                                                                             newPoint,
                                                                             statusCallback));
    directCommand->redo();
    owner_->flushPendingMapSceneRefreshAfterCommand();
}

void MapEditorCanvasEditController::recordLineAreaVertexMove(int lineNumber,
                                            const QString &kind,
                                            int vertexIndex,
                                            const QPointF &oldPoint,
                                            const QPointF &newPoint)
{
    if (lineNumber <= 0 || vertexIndex < 0 || !sourcePointsDifferForCommands(oldPoint, newPoint) || owner_->textEditor_ == nullptr) {
        return;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    const QString rewriteKind = normalizedKind.startsWith(QStringLiteral("line"))
        ? QStringLiteral("line")
        : (normalizedKind.startsWith(QStringLiteral("area")) ? QStringLiteral("area") : normalizedKind);
    if (rewriteKind != QStringLiteral("line") && rewriteKind != QStringLiteral("area")) {
        owner_->toolbarStatusNote_ = owner_->tr("Geometry move failed: unsupported geometry kind '%1'.").arg(kind);
        owner_->refreshToolbarSummary();
        return;
    }

    QVector<MapLineAreaVertexSecondaryMove> secondaryMoves;
    if (rewriteKind == QStringLiteral("line")) {
        const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(), lineNumber);
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

    auto statusCallback = [owner = owner_](const QString &statusMessage) {
        owner->toolbarStatusNote_ = statusMessage;
        owner->refreshToolbarSummary();
    };

    if (owner_->undoStack_ != nullptr) {
        const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
        owner_->undoStack_->push(createMapLineAreaVertexMoveCommand(owner_->textEditor_,
                                                           lineNumber,
                                                           rewriteKind,
                                                           vertexIndex,
                                                           oldPoint,
                                                           newPoint,
                                                           secondaryMoves,
                                                           statusCallback));
        owner_->flushPendingMapSceneRefreshAfterCommand();
        return;
    }

    const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
    std::unique_ptr<QUndoCommand> directCommand(createMapLineAreaVertexMoveCommand(owner_->textEditor_,
                                                                             lineNumber,
                                                                             rewriteKind,
                                                                             vertexIndex,
                                                                             oldPoint,
                                                                             newPoint,
                                                                             secondaryMoves,
                                                                             statusCallback));
    directCommand->redo();
    owner_->flushPendingMapSceneRefreshAfterCommand();
}

void MapEditorCanvasEditController::recordPointOrientationHandleChange(int lineNumber, qreal orientationDegrees)
{
    if (lineNumber <= 0 || owner_->textEditor_ == nullptr) {
        return;
    }

    owner_->pendingMapClickSelection_ = false;
    owner_->pendingMapClickLineNumber_ = 0;
    owner_->pendingMapClickSourceVertexIndex_ = -1;
    owner_->pendingMapClickGeometryKind_.clear();

    const QString beforeText = owner_->textEditor_->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewritePointOrientation(&afterText,
                                                        lineNumber,
                                                        true,
                                                        orientationDegrees,
                                                        &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Point orientation update failed.")
            : owner_->tr("Point orientation update failed: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    owner_->textEditor_->replaceTextForCommand(afterText);
    recordSourceTextSnapshot(owner_->tr("Edit Point Orientation"), beforeText, afterText, lineNumber);
    restorePointSelection(lineNumber);
    QTimer::singleShot(0, owner_, [owner = owner_, lineNumber]() {
        owner->restorePointSelection(lineNumber);
    });
    owner_->toolbarStatusNote_ = owner_->tr("Updated point orientation to %1 degrees.")
        .arg(QString::number(normalizeOrientationDegreesForMapDetails(orientationDegrees), 'f', 1));
    owner_->refreshToolbarSummary();
}

void MapEditorCanvasEditController::recordLinePointLeftHandleChange(int lineNumber,
                                                   int sourceVertexIndex,
                                                   qreal orientationDegrees,
                                                   qreal leftSize)
{
    if (lineNumber <= 0 || sourceVertexIndex < 0 || owner_->textEditor_ == nullptr) {
        return;
    }

    owner_->pendingMapClickSelection_ = false;
    owner_->pendingMapClickLineNumber_ = 0;
    owner_->pendingMapClickSourceVertexIndex_ = -1;
    owner_->pendingMapClickGeometryKind_.clear();

    const QString beforeText = owner_->textEditor_->text();
    QString afterText = beforeText;
    QString errorMessage;
    if (!TherionDocumentEditor::rewriteLinePointOrientation(&afterText,
                                                            lineNumber,
                                                            sourceVertexIndex,
                                                            true,
                                                            orientationDegrees,
                                                            &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Line point orientation update failed.")
            : owner_->tr("Line point orientation update failed: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        return;
    }

    if (!TherionDocumentEditor::rewriteLinePointLeftSize(&afterText,
                                                         lineNumber,
                                                         sourceVertexIndex,
                                                         true,
                                                         leftSize,
                                                         &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Line point l-size update failed.")
            : owner_->tr("Line point l-size update failed: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        return;
    }

    if (afterText == beforeText) {
        return;
    }

    owner_->textEditor_->replaceTextForCommand(afterText);
    recordSourceTextSnapshot(owner_->tr("Edit Line Point Options"), beforeText, afterText, lineNumber);
    restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
    QTimer::singleShot(0, owner_, [owner = owner_, lineNumber, sourceVertexIndex]() {
        owner->restoreLineAnchorSelection(lineNumber, sourceVertexIndex);
    });
    owner_->toolbarStatusNote_ = owner_->tr("Updated line point orientation %1 deg and l-size %2.")
        .arg(QString::number(orientationDegrees, 'f', 1),
             QString::number(leftSize, 'f', 1));
    owner_->refreshToolbarSummary();
}

void MapEditorCanvasEditController::restorePointSelection(int lineNumber)
{
    if (owner_->mapScene_ == nullptr || lineNumber <= 0) {
        return;
    }

    auto selectedItemIt = owner_->mapItemsByLine_.find(lineNumber);
    if (selectedItemIt == owner_->mapItemsByLine_.end()) {
        return;
    }

    auto *pointItem = dynamic_cast<MapEditablePointItem *>(selectedItemIt.value());
    if (pointItem == nullptr) {
        return;
    }

    {
        const QScopedValueRollback<bool> selectionGuard(owner_->updatingSelection_, true);
        owner_->mapScene_->clearSelection();
        pointItem->setVisible(true);
        pointItem->setSelected(true);
    }

    owner_->selectedObjectLineNumber_ = lineNumber;
    owner_->selectedObjectVertexIndex_ = -1;
    owner_->selectedObjectKind_ = QStringLiteral("point");
    owner_->selectedObjectCoordinate_ = pointItem->sourcePoint();
    owner_->updateGeometrySelectionPresentation();
    owner_->updateCommandSurfaceState();
    owner_->updateHelpPanel();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorCanvasEditController::restoreLineAnchorSelection(int lineNumber, int sourceVertexIndex)
{
    if (owner_->mapScene_ == nullptr || lineNumber <= 0 || sourceVertexIndex < 0) {
        return;
    }

    MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(owner_->mapScene_,
                                                                        lineNumber,
                                                                        sourceVertexIndex,
                                                                        QStringLiteral("line"));
    if (ownerAnchor == nullptr) {
        return;
    }

    {
        const QScopedValueRollback<bool> selectionGuard(owner_->updatingSelection_, true);
        owner_->mapScene_->clearSelection();
        ownerAnchor->setVisible(true);
        ownerAnchor->setSelected(true);
    }

    owner_->selectedObjectLineNumber_ = lineNumber;
    owner_->selectedObjectVertexIndex_ = sourceVertexIndex;
    owner_->selectedObjectKind_ = QStringLiteral("line");
    owner_->selectedObjectCoordinate_ = owner_->sourcePointFromScenePosition(ownerAnchor->pos());
    owner_->updateGeometrySelectionPresentation();
    owner_->updateCommandSurfaceState();
    owner_->updateHelpPanel();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorCanvasEditController::recordSourceTextSnapshot(const QString &label,
                                            const QString &beforeText,
                                            const QString &afterText,
                                            int insertedLineNumber)
{
    if (owner_->textEditor_ == nullptr || beforeText == afterText) {
        return;
    }

    auto statusCallback = [owner = owner_](const QString &statusMessage) {
        owner->toolbarStatusNote_ = statusMessage;
        owner->refreshToolbarSummary();
    };

    if (owner_->undoStack_ != nullptr) {
        const QScopedValueRollback<bool> commandGuard(owner_->mapCommandApplyInProgress_, true);
        owner_->undoStack_->push(createMapSourceTextSnapshotCommand(owner_->textEditor_,
                                                        label,
                                                        beforeText,
                                                        afterText,
                                                        insertedLineNumber,
                                                        statusCallback));
        owner_->flushPendingMapSceneRefreshAfterCommand();
    }
}

bool MapEditorCanvasEditController::insertLineVertexFromSelection()
{
    if (owner_->mapScene_ == nullptr || owner_->textEditor_ == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = owner_->mapScene_->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(), lineNumber);
    if (!lineFeature.has_value()) {
        owner_->toolbarStatusNote_ = owner_->tr("Insert vertex failed: line geometry could not be resolved.");
        owner_->refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (anchorIndex < 0) {
        owner_->toolbarStatusNote_ = owner_->tr("Insert vertex failed: selected line anchor could not be resolved.");
        owner_->refreshToolbarSummary();
        return true;
    }

    int segmentStartIndex = -1;
    if (anchorIndex < lineFeature->lineVertices.size() - 1) {
        segmentStartIndex = anchorIndex;
    } else if (anchorIndex > 0) {
        segmentStartIndex = anchorIndex - 1;
    }
    if (segmentStartIndex < 0) {
        owner_->toolbarStatusNote_ = owner_->tr("Insert vertex failed: line needs at least one editable segment.");
        owner_->refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    int insertedIndex = -1;
    if (!insertLineVertexByDeCasteljau(&editedVertices, segmentStartIndex, 0.5, &insertedIndex)) {
        owner_->toolbarStatusNote_ = owner_->tr("Insert vertex failed: segment split could not be computed.");
        owner_->refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!owner_->textEditor_->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Insert vertex failed.")
            : owner_->tr("Insert vertex failed: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        return true;
    }

    owner_->toolbarStatusNote_ = insertedIndex >= 0
        ? owner_->tr("Inserted line vertex %1 on source line %2.").arg(insertedIndex + 1).arg(lineNumber)
        : owner_->tr("Inserted line vertex on source line %1.").arg(lineNumber);
    owner_->refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::removeLineVertexFromSelection()
{
    if (owner_->mapScene_ == nullptr || owner_->textEditor_ == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = owner_->mapScene_->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(), lineNumber);
    if (!lineFeature.has_value()) {
        owner_->toolbarStatusNote_ = owner_->tr("Delete vertex failed: line geometry could not be resolved.");
        owner_->refreshToolbarSummary();
        return true;
    }

    const int anchorIndex = lineVertexIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (anchorIndex < 0) {
        owner_->toolbarStatusNote_ = owner_->tr("Delete vertex failed: selected line anchor could not be resolved.");
        owner_->refreshToolbarSummary();
        return true;
    }

    QVector<MapGeometryFeature::TH2LineVertex> editedVertices = lineFeature->lineVertices;
    if (!removeLineVertexWithReconnect(&editedVertices, anchorIndex)) {
        owner_->toolbarStatusNote_ = owner_->tr("Delete vertex failed: only middle anchors can be removed while keeping a valid line.");
        owner_->refreshToolbarSummary();
        return true;
    }

    const QStringList coordinateRows = coordinateRowsForLineVertices(editedVertices);
    QString errorMessage;
    if (!owner_->textEditor_->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Delete vertex failed.")
            : owner_->tr("Delete vertex failed: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        return true;
    }

    owner_->toolbarStatusNote_ = owner_->tr("Removed line vertex %1 on source line %2.").arg(anchorIndex + 1).arg(lineNumber);
    owner_->refreshToolbarSummary();
    return true;
}

bool MapEditorCanvasEditController::toggleLineVertexSmoothFromSelection()
{
    if (owner_->mapScene_ == nullptr || owner_->textEditor_ == nullptr) {
        return false;
    }

    const QList<QGraphicsItem *> selectedItems = owner_->mapScene_->selectedItems();
    if (selectedItems.size() != 1) {
        return false;
    }

    auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(selectedItems.first());
    if (vertexItem == nullptr || vertexItem->geometryKind() != QStringLiteral("line")) {
        return false;
    }

    const int lineNumber = vertexItem->lineNumber();
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(), lineNumber);
    if (!lineFeature.has_value()) {
        owner_->toolbarStatusNote_ = owner_->tr("Toggle smooth failed: line geometry could not be resolved.");
        owner_->refreshToolbarSummary();
        return true;
    }

    const int ownerIndex = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), vertexItem->vertexIndex());
    if (ownerIndex < 0 || ownerIndex >= lineFeature->lineVertices.size()) {
        owner_->toolbarStatusNote_ = owner_->tr("Toggle smooth failed: selected line vertex could not be resolved.");
        owner_->refreshToolbarSummary();
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
    if (!owner_->textEditor_->rewriteLineCoordinateRows(lineNumber, coordinateRows, &errorMessage)) {
        owner_->toolbarStatusNote_ = errorMessage.isEmpty()
            ? owner_->tr("Toggle smooth failed.")
            : owner_->tr("Toggle smooth failed: %1").arg(errorMessage);
        owner_->refreshToolbarSummary();
        return true;
    }

    owner_->toolbarStatusNote_ = target.isSmooth
        ? owner_->tr("Line vertex %1 on source line %2 set to smooth.").arg(ownerIndex + 1).arg(lineNumber)
        : owner_->tr("Line vertex %1 on source line %2 set to corner (smooth off).").arg(ownerIndex + 1).arg(lineNumber);
    owner_->refreshToolbarSummary();
    return true;
}

QGraphicsRectItem *MapEditorCanvasEditController::selectedDraftGeometryItem() const
{
    if (owner_->mapScene_ == nullptr) {
        return nullptr;
    }

    const QList<QGraphicsItem *> selectedItems = owner_->mapScene_->selectedItems();
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
    return new MapDraftGeometryItem(owner_->nextDraftGeometryId_++, kind);
}

void MapEditorCanvasEditController::addDraftGeometryItem(QGraphicsRectItem *item, const QPointF &position)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    draftItem->setPos(position);
    draftItem->setMoveCommittedCallback([owner = owner_, draftItem](int, const QPointF &oldPosition, const QPointF &newPosition) {
        owner->recordDraftMove(draftItem, oldPosition, newPosition);
    });
    draftItem->setVisibilityCommittedCallback([owner = owner_, draftItem](int, bool oldVisible, bool newVisible) {
        owner->recordDraftVisibility(draftItem, oldVisible, newVisible);
    });
    if (owner_->undoStack_ != nullptr) {
        owner_->undoStack_->push(createMapDraftAddCommand(owner_->mapScene_, &owner_->draftGeometryItems_, draftItem, position));
    } else {
        if (owner_->mapScene_ != nullptr) {
            owner_->mapScene_->addItem(draftItem);
        }
        owner_->draftGeometryItems_.append(draftItem);
    }

    owner_->updateCommandSurfaceState();
}

void MapEditorCanvasEditController::removeDraftGeometryItem(QGraphicsRectItem *item)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr) {
        return;
    }

    owner_->draftGeometryItems_.removeAll(draftItem);
    if (owner_->mapScene_ != nullptr) {
        owner_->mapScene_->removeItem(draftItem);
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

    const QRectF previewBounds = owner_->mapPreviewBounds();
    const QRectF sourceBounds = owner_->mapSourceBoundsForCurrentDocument();
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
    if (draftItem == nullptr || owner_->undoStack_ == nullptr || oldPosition == newPosition) {
        return;
    }

    owner_->undoStack_->push(createMapDraftMoveCommand(draftItem, oldPosition, newPosition));
}

void MapEditorCanvasEditController::recordDraftVisibility(QGraphicsRectItem *item, bool oldVisible, bool newVisible)
{
    auto *draftItem = dynamic_cast<MapDraftGeometryItem *>(item);
    if (draftItem == nullptr || owner_->undoStack_ == nullptr || oldVisible == newVisible) {
        return;
    }

    owner_->undoStack_->push(createMapDraftVisibilityCommand(draftItem, oldVisible, newVisible));
}

}
