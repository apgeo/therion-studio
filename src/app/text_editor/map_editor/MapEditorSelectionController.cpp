#include "MapEditorSelectionController.h"

#include "../TextEditorTab.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "MapEditorTab.h"
#include "../../../core/TherionDocumentParser.h"

#include <QCursor>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScopedValueRollback>
#include <QWidget>

#include <algorithm>
#include <limits>
#include <optional>

namespace TherionStudio
{
namespace
{
bool isInteractiveMapSelectionItem(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return false;
    }
    if (!item->isVisible()) {
        return false;
    }
    if (!(item->flags() & QGraphicsItem::ItemIsSelectable)) {
        return false;
    }
    if (item->acceptedMouseButtons() == Qt::NoButton) {
        return false;
    }
    return true;
}

int mapSelectionHitPriority(const QGraphicsItem *item)
{
    if (item == nullptr) {
        return 100;
    }

    if (dynamic_cast<const MapEditablePointItem *>(item) != nullptr) {
        return 0;
    }
    if (auto *vertexItem = dynamic_cast<const MapEditableGeometryVertexItem *>(item)) {
        const QString geometryKind = vertexItem->geometryKind().trimmed().toLower();
        if (geometryKind == QStringLiteral("line") || geometryKind == QStringLiteral("area")) {
            return 0;
        }
        if (geometryKind.startsWith(QStringLiteral("line control"))) {
            return 2;
        }
    }

    const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
    switch (subtype) {
    case kMapSceneSelectionSubtypeLineAnchor:
    case kMapSceneSelectionSubtypeAreaVertex:
        return 0;
    case kMapSceneSelectionSubtypeLineControl:
        return 2;
    case kMapSceneSelectionSubtypePointOrientationHandle:
        return 2;
    case kMapSceneSelectionSubtypeLineControlConnector:
        return 3;
    case kMapSceneSelectionSubtypeLineDetail:
        return 4;
    default:
        break;
    }

    return 1;
}

QGraphicsItem *preferredMapHitItem(const QList<QGraphicsItem *> &hitItems, bool requireSelected = false)
{
    QGraphicsItem *bestItem = nullptr;
    int bestPriority = std::numeric_limits<int>::max();
    for (QGraphicsItem *item : hitItems) {
        if (!isInteractiveMapSelectionItem(item)) {
            continue;
        }
        if (requireSelected && !item->isSelected()) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber <= 0) {
            continue;
        }

        const int priority = mapSelectionHitPriority(item);
        if (priority < bestPriority) {
            bestPriority = priority;
            bestItem = item;
            if (bestPriority == 0) {
                break;
            }
        }
    }
    return bestItem;
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

MapEditorSelectionController::MapEditorSelectionController(MapEditorTab *owner)
    : owner_(owner)
{
}

void MapEditorSelectionController::handleMapSceneSelectionChanged()
{
    if (owner_->updatingSelection_ || owner_->mapScene_ == nullptr) {
        owner_->updateHelpPanel();
        owner_->refreshObjectDetailsPanel();
        return;
    }

    const QList<QGraphicsItem *> selectedItems = owner_->mapScene_->selectedItems();
    if (selectedItems.isEmpty()) {
        owner_->selectedObjectLineNumber_ = 0;
        owner_->selectedObjectVertexIndex_ = -1;
        owner_->selectedObjectKind_.clear();
        owner_->selectedObjectCoordinate_.reset();
        updateGeometrySelectionPresentation();
        owner_->updateHelpPanel();
        owner_->refreshObjectDetailsPanel();
        return;
    }

    int selectedLineNumber = 0;
    int selectedSourceVertexIndex = -1;
    QString selectedGeometryKind;
    std::optional<QPointF> selectedPointSource;
    MapCardItem *selectedCard = nullptr;
    QList<QGraphicsItem *> selectedLineItems;
    QSet<int> selectedLineNumbers;
    selectedLineItems.reserve(selectedItems.size());
    for (QGraphicsItem *item : selectedItems) {
        if (!isInteractiveMapSelectionItem(item)) {
            continue;
        }
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber <= 0) {
            continue;
        }
        selectedLineItems.append(item);
        selectedLineNumbers.insert(lineNumber);
    }

    QGraphicsItem *primarySelectedItem = nullptr;
    const bool usePendingClickSelection = owner_->pendingMapClickSelection_
        && owner_->pendingMapClickElapsed_.isValid()
        && owner_->pendingMapClickElapsed_.elapsed() <= 1000;
    if (usePendingClickSelection && owner_->pendingMapClickLineNumber_ > 0) {
        if (owner_->pendingMapClickSourceVertexIndex_ >= 0) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item == nullptr) {
                    continue;
                }
                if (item->data(kMapSceneLineNumberRole).toInt() != owner_->pendingMapClickLineNumber_) {
                    continue;
                }
                auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
                if (vertexItem == nullptr) {
                    continue;
                }
                if (vertexItem->vertexIndex() != owner_->pendingMapClickSourceVertexIndex_) {
                    continue;
                }
                if (!owner_->pendingMapClickGeometryKind_.isEmpty()
                    && !vertexItem->geometryKind().startsWith(owner_->pendingMapClickGeometryKind_)) {
                    continue;
                }
                primarySelectedItem = item;
                break;
            }
        }
        if (primarySelectedItem == nullptr) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item != nullptr && item->data(kMapSceneLineNumberRole).toInt() == owner_->pendingMapClickLineNumber_) {
                    primarySelectedItem = item;
                    break;
                }
            }
        }
    }
    if (owner_->mapView_ != nullptr && owner_->mapView_->viewport() != nullptr) {
        if (primarySelectedItem == nullptr && usePendingClickSelection) {
            const QPointF scenePos = owner_->pendingMapClickScenePosition_;
            const QList<QGraphicsItem *> hitItems = owner_->mapScene_->items(scenePos,
                                                                     Qt::IntersectsItemShape,
                                                                     Qt::DescendingOrder,
                                                                     owner_->mapView_->transform());
            primarySelectedItem = preferredMapHitItem(hitItems, true);
        } else if (primarySelectedItem == nullptr) {
            const QPoint viewportPos = owner_->mapView_->viewport()->mapFromGlobal(QCursor::pos());
            if (owner_->mapView_->viewport()->rect().contains(viewportPos)) {
                const QPointF scenePos = owner_->mapView_->mapToScene(viewportPos);
                const QList<QGraphicsItem *> hitItems = owner_->mapScene_->items(scenePos,
                                                                         Qt::IntersectsItemShape,
                                                                         Qt::DescendingOrder,
                                                                         owner_->mapView_->transform());
                primarySelectedItem = preferredMapHitItem(hitItems, true);
            }
        }
    }

    if (primarySelectedItem == nullptr && !selectedLineItems.isEmpty()) {
        if (selectedLineNumbers.size() == 1 && selectedLineItems.size() > 1) {
            QGraphicsItem *highestZItem = selectedLineItems.first();
            qreal highestZ = highestZItem != nullptr ? highestZItem->zValue() : 0.0;
            for (QGraphicsItem *item : selectedLineItems) {
                if (item != nullptr && item->zValue() > highestZ) {
                    highestZ = item->zValue();
                    highestZItem = item;
                }
            }
            primarySelectedItem = highestZItem;
        } else {
            primarySelectedItem = selectedLineItems.first();
        }
    }

    owner_->pendingMapClickSelection_ = false;
    owner_->pendingMapClickLineNumber_ = 0;
    owner_->pendingMapClickSourceVertexIndex_ = -1;
    owner_->pendingMapClickGeometryKind_.clear();

    if (primarySelectedItem != nullptr) {
        selectedLineNumber = primarySelectedItem->data(kMapSceneLineNumberRole).toInt();
        selectedCard = dynamic_cast<MapCardItem *>(primarySelectedItem);
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem)) {
            selectedSourceVertexIndex = vertexItem->vertexIndex();
            selectedGeometryKind = vertexItem->geometryKind();
        } else if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(primarySelectedItem)) {
            selectedPointSource = pointItem->sourcePoint();
        } else if (primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypePointOrientationHandle) {
            if (auto selectedItemIt = owner_->mapItemsByLine_.find(selectedLineNumber);
                selectedItemIt != owner_->mapItemsByLine_.end()) {
                if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(selectedItemIt.value())) {
                    const QScopedValueRollback<bool> selectionGuard(owner_->updatingSelection_, true);
                    pointItem->setSelected(true);
                    selectedPointSource = pointItem->sourcePoint();
                }
            }
        } else if (primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypeLineControlConnector) {
            const int ownerVertexIndex = primarySelectedItem->data(kMapSceneOwnerVertexRole).toInt();
            if (ownerVertexIndex >= 0) {
                selectedSourceVertexIndex = ownerVertexIndex;
                selectedGeometryKind = QStringLiteral("line");
            }
        }
    }

    int selectedSourceLineNumber = selectedLineNumber;
    if (selectedLineNumber > 0 && owner_->textEditor_ != nullptr) {
        const QScopedValueRollback<bool> syncGuard(owner_->mapSelectionDrivenTextNavigationInProgress_, true);
        if (selectedPointSource.has_value()) {
            const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(owner_->textEditor_->text());
            const std::optional<int> pointLineNumber =
                sourcePointLineNumberForSelection(parsedLines, selectedPointSource.value());
            if (pointLineNumber.has_value()) {
                selectedSourceLineNumber = pointLineNumber.value();
                owner_->textEditor_->goToLineColumn(pointLineNumber.value(), 1);
            } else if (selectedLineNumber != owner_->currentLineNumber()) {
                owner_->textEditor_->goToLine(selectedLineNumber);
            }
        } else if (selectedSourceVertexIndex >= 0) {
            const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(owner_->textEditor_->text());
            const std::optional<SourceVertexTextReference> sourceReference =
                sourceVertexTextReferenceForSelection(parsedLines,
                                                      selectedLineNumber,
                                                      selectedGeometryKind,
                                                      selectedSourceVertexIndex);
            if (sourceReference.has_value()) {
                if (owner_->textEditor_->currentLineNumber() != sourceReference->lineNumber
                    || owner_->textEditor_->currentColumnNumber() != sourceReference->xStartColumn) {
                    owner_->textEditor_->goToLineColumn(sourceReference->lineNumber, sourceReference->xStartColumn);
                }
            } else if (selectedLineNumber != owner_->currentLineNumber()) {
                owner_->textEditor_->goToLine(selectedLineNumber);
            }
        } else if (selectedLineNumber != owner_->currentLineNumber()) {
            owner_->textEditor_->goToLine(selectedLineNumber);
        }
    }

    owner_->selectedObjectLineNumber_ = selectedSourceLineNumber;
    owner_->selectedObjectVertexIndex_ = selectedSourceVertexIndex;
    owner_->selectedObjectCoordinate_.reset();
    if (selectedPointSource.has_value()) {
        owner_->selectedObjectKind_ = QStringLiteral("point");
        owner_->selectedObjectCoordinate_ = selectedPointSource.value();
    } else if (selectedSourceVertexIndex >= 0) {
        const QString normalizedGeometryKind = selectedGeometryKind.trimmed().toLower();
        if (normalizedGeometryKind.startsWith(QStringLiteral("line"))) {
            owner_->selectedObjectKind_ = QStringLiteral("line");
        } else if (normalizedGeometryKind.startsWith(QStringLiteral("area"))) {
            owner_->selectedObjectKind_ = QStringLiteral("area");
        } else {
            owner_->selectedObjectKind_ = normalizedGeometryKind;
        }
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem)) {
            owner_->selectedObjectCoordinate_ = owner_->sourcePointFromScenePosition(vertexItem->pos());
        } else if (selectedLineNumber > 0 && selectedSourceVertexIndex >= 0 && owner_->mapScene_ != nullptr) {
            if (MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(owner_->mapScene_,
                                                                                   selectedLineNumber,
                                                                                   selectedSourceVertexIndex,
                                                                                   QStringLiteral("line"))) {
                const QScopedValueRollback<bool> selectionGuard(owner_->updatingSelection_, true);
                ownerAnchor->setSelected(true);
                owner_->selectedObjectCoordinate_ = owner_->sourcePointFromScenePosition(ownerAnchor->pos());
            }
        }
    } else if (selectedLineNumber > 0 && owner_->textEditor_ != nullptr) {
        if (const std::optional<MapGeometryFeature> feature = lineFeatureForLineNumber(owner_->textEditor_->text(), selectedLineNumber);
            feature.has_value()) {
            if (feature->kind == MapGeometryFeature::Kind::Line) {
                owner_->selectedObjectKind_ = QStringLiteral("line");
            } else if (feature->kind == MapGeometryFeature::Kind::Area) {
                owner_->selectedObjectKind_ = QStringLiteral("area");
            } else if (feature->kind == MapGeometryFeature::Kind::Point) {
                owner_->selectedObjectKind_ = QStringLiteral("point");
            }
        }
        if (owner_->selectedObjectKind_.isEmpty()) {
            owner_->selectedObjectKind_ = QStringLiteral("object");
        }
    } else {
        owner_->selectedObjectKind_.clear();
    }

    if (selectedCard != nullptr) {
        updateGeometrySelectionPresentation();
        owner_->updateCommandSurfaceState();
        owner_->updateHelpPanel();
        owner_->refreshObjectDetailsPanel();
        return;
    }

    updateGeometrySelectionPresentation();
    owner_->updateCommandSurfaceState();
    owner_->updateHelpPanel();
    owner_->refreshObjectDetailsPanel();
}

void MapEditorSelectionController::syncMapSelectionFromTextCursor(int lineNumber, int columnNumber)
{
    owner_->lastCursorSyncedLine_ = lineNumber;
    owner_->lastCursorSyncedColumn_ = columnNumber;

    if (owner_->mapScene_ == nullptr || owner_->textEditor_ == nullptr || lineNumber <= 0) {
        return;
    }
    if (owner_->suppressedInspectorAutoReselectLineNumbers_.contains(lineNumber)) {
        return;
    }
    if (!owner_->suppressedInspectorAutoReselectLineNumbers_.isEmpty()) {
        owner_->suppressedInspectorAutoReselectLineNumbers_.clear();
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(owner_->textEditor_->text());
    if (const std::optional<QSet<int>> scrapLines = scrapObjectLinesForCursor(parsedLines, lineNumber);
        scrapLines.has_value()) {
        if (!scrapLines->isEmpty()) {
            selectMapLines(scrapLines.value());
        } else {
            selectMapLine(lineNumber);
        }
        return;
    }

    const CursorGeometrySelection cursorSelection = cursorGeometrySelectionForTextCursor(parsedLines,
                                                                                         lineNumber,
                                                                                         columnNumber);
    if (cursorSelection.featureLineNumber <= 0) {
        selectMapLine(lineNumber);
        return;
    }

    selectMapLine(cursorSelection.featureLineNumber);
    if (!cursorSelection.sourceVertexReference.has_value()) {
        return;
    }

    const int sourceVertexIndex = cursorSelection.sourceVertexReference->sourceVertexIndex;
    if (sourceVertexIndex < 0 || owner_->mapScene_ == nullptr) {
        return;
    }

    const QString geometryKind = cursorSelection.geometryKind.trimmed().toLower();
    const bool lineGeometry = geometryKind == QStringLiteral("line");
    const bool areaGeometry = geometryKind == QStringLiteral("area");
    if (!lineGeometry && !areaGeometry) {
        return;
    }

    MapEditableGeometryVertexItem *targetItem = nullptr;
    if (lineGeometry) {
        int ownerSourceVertexIndex = -1;
        if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(owner_->textEditor_->text(),
                                                                                            cursorSelection.featureLineNumber);
            lineFeature.has_value()) {
            const int ownerVertexOrder = lineVertexOwnerIndexForSourceVertex(lineFeature.value(), sourceVertexIndex);
            if (ownerVertexOrder >= 0 && ownerVertexOrder < lineFeature->lineVertices.size()) {
                const MapGeometryFeature::TH2LineVertex &ownerVertex = lineFeature->lineVertices.at(ownerVertexOrder);
                ownerSourceVertexIndex = ownerVertex.anchorSourceVertexIndex >= 0
                    ? ownerVertex.anchorSourceVertexIndex
                    : ownerVertexOrder;
            }
        }

        owner_->updatingSelection_ = true;
        if (ownerSourceVertexIndex >= 0) {
            if (MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(owner_->mapScene_,
                                                                                     cursorSelection.featureLineNumber,
                                                                                     ownerSourceVertexIndex,
                                                                                     QStringLiteral("line"))) {
                ownerAnchor->setSelected(true);
            }
        }
        owner_->updatingSelection_ = false;
        updateGeometrySelectionPresentation();

        targetItem = findGeometryVertexItem(owner_->mapScene_,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("line"));
    } else {
        targetItem = findGeometryVertexItem(owner_->mapScene_,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("area"));
    }

    if (targetItem != nullptr) {
        owner_->updatingSelection_ = true;
        targetItem->setSelected(true);
        owner_->updatingSelection_ = false;
        updateGeometrySelectionPresentation();
        owner_->updateCommandSurfaceState();
        owner_->updateHelpPanel();
    }
}

void MapEditorSelectionController::updateGeometrySelectionPresentation()
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    QSet<int> selectedLines;
    QHash<int, QSet<int>> selectedLineControlOwnersByLine;
    const QList<QGraphicsItem *> selectedItems = owner_->mapScene_->selectedItems();
    for (QGraphicsItem *item : selectedItems) {
        if (item == nullptr) {
            continue;
        }

        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        if (lineNumber > 0) {
            selectedLines.insert(lineNumber);
        }

        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        if (subtype == kMapSceneSelectionSubtypeLineAnchor
            || subtype == kMapSceneSelectionSubtypeLineControl
            || subtype == kMapSceneSelectionSubtypeLineControlConnector) {
            const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
            if (lineNumber > 0 && ownerVertexIndex >= 0) {
                selectedLineControlOwnersByLine[lineNumber].insert(ownerVertexIndex);
            }
        }
    }

    const auto sceneItems = owner_->mapScene_->items();
    for (QGraphicsItem *item : sceneItems) {
        if (item == nullptr) {
            continue;
        }
        if (!item->data(kMapSceneSelectionGatedRole).toBool()) {
            continue;
        }

        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        bool visible = lineNumber > 0
            && !owner_->hiddenInspectorObjectLines_.contains(lineNumber)
            && selectedLines.contains(lineNumber);
        if (visible) {
            const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
            if (subtype == kMapSceneSelectionSubtypeLineControl
                || subtype == kMapSceneSelectionSubtypeLineControlConnector) {
                const int ownerVertexIndex = item->data(kMapSceneOwnerVertexRole).toInt();
                visible = ownerVertexIndex >= 0
                    && selectedLineControlOwnersByLine.value(lineNumber).contains(ownerVertexIndex);
            }
        }

        item->setVisible(visible);
    }
}

void MapEditorSelectionController::selectMapLine(int lineNumber, bool centerOnSelection)
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    owner_->pendingMapClickSelection_ = false;
    owner_->pendingMapClickLineNumber_ = 0;
    owner_->pendingMapClickSourceVertexIndex_ = -1;
    owner_->pendingMapClickGeometryKind_.clear();
    owner_->updatingSelection_ = true;
    owner_->mapScene_->clearSelection();

    auto selectedItemIt = owner_->mapItemsByLine_.find(lineNumber);
    if (selectedItemIt != owner_->mapItemsByLine_.end() && selectedItemIt.value() != nullptr) {
        selectedItemIt.value()->setSelected(true);
        if (centerOnSelection && !owner_->autoFitEnabled_) {
            owner_->mapView_->centerOn(selectedItemIt.value());
        }
    }

    owner_->updatingSelection_ = false;
    updateGeometrySelectionPresentation();
}

void MapEditorSelectionController::selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection)
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    owner_->pendingMapClickSelection_ = false;
    owner_->pendingMapClickLineNumber_ = 0;
    owner_->pendingMapClickSourceVertexIndex_ = -1;
    owner_->pendingMapClickGeometryKind_.clear();
    owner_->updatingSelection_ = true;
    owner_->mapScene_->clearSelection();

    QList<int> sortedLines = lineNumbers.values();
    std::sort(sortedLines.begin(), sortedLines.end());
    QGraphicsItem *firstSelectedItem = nullptr;
    for (const int lineNumber : std::as_const(sortedLines)) {
        auto selectedItemIt = owner_->mapItemsByLine_.find(lineNumber);
        if (selectedItemIt == owner_->mapItemsByLine_.end() || selectedItemIt.value() == nullptr) {
            continue;
        }
        selectedItemIt.value()->setSelected(true);
        if (firstSelectedItem == nullptr) {
            firstSelectedItem = selectedItemIt.value();
        }
    }

    if (centerOnSelection && firstSelectedItem != nullptr && !owner_->autoFitEnabled_ && owner_->mapView_ != nullptr) {
        owner_->mapView_->centerOn(firstSelectedItem);
    }

    owner_->updatingSelection_ = false;
    updateGeometrySelectionPresentation();
}

}
