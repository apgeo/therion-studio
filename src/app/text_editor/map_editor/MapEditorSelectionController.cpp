#include "MapEditorSelectionController.h"

#include "../TextEditorTab.h"
#include "MapEditorSceneInternals.h"
#include "MapEditorSceneSupport.h"
#include "MapEditorSourceReferenceResolver.h"
#include "../../../core/TherionDocumentParser.h"

#include <QCursor>
#include <QElapsedTimer>
#include <QGraphicsItem>
#include <QGraphicsPathItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScopedValueRollback>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

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
    case kMapSceneSelectionSubtypeAreaFill:
        return 5;
    default:
        break;
    }

    return 1;
}

bool genericPathItemContainsStrokedHit(const QGraphicsItem *item, const QPointF &scenePosition)
{
    const auto *pathItem = dynamic_cast<const QGraphicsPathItem *>(item);
    if (pathItem == nullptr) {
        return true;
    }

    const QPainterPath path = pathItem->path();
    const QPointF localPosition = pathItem->mapFromScene(scenePosition);
    const qreal tolerance = std::max<qreal>(pathItem->pen().widthF() + 4.0, 6.0);
    if (!path.boundingRect().adjusted(-tolerance, -tolerance, tolerance, tolerance).contains(localPosition)) {
        return false;
    }

    const qreal pathLength = path.length();
    if (pathLength <= 0.0) {
        return false;
    }

    const int sampleCount = qBound(24, static_cast<int>(std::ceil(pathLength / std::max<qreal>(tolerance * 0.5, 2.0))), 512);
    for (int index = 0; index <= sampleCount; ++index) {
        const qreal percent = static_cast<qreal>(index) / static_cast<qreal>(sampleCount);
        const QPointF pathPoint = path.pointAtPercent(percent);
        const qreal dx = pathPoint.x() - localPosition.x();
        const qreal dy = pathPoint.y() - localPosition.y();
        if (std::hypot(dx, dy) <= tolerance) {
            return true;
        }
    }
    return false;
}

QGraphicsItem *preferredMapHitItem(const QList<QGraphicsItem *> &hitItems,
                                   bool requireSelected = false,
                                   std::optional<QPointF> scenePosition = std::nullopt)
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

        const int subtype = item->data(kMapSceneSelectionSubtypeRole).toInt();
        if (scenePosition.has_value()
            && subtype == kMapSceneSelectionSubtypeGeneric
            && !genericPathItemContainsStrokedHit(item, scenePosition.value())) {
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

MapEditorSelectionController::MapEditorSelectionController(MapEditorSelectionContext context)
    : context_(std::move(context))
{
}

void MapEditorSelectionController::handleMapSceneSelectionChanged()
{
    if ((*context_.updatingSelection) || context_.scene == nullptr) {
        context_.updateHelpPanel();
        context_.refreshObjectDetailsPanel();
        return;
    }

    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
    if (selectedItems.isEmpty()) {
        (*context_.selectedObjectLineNumber) = 0;
        (*context_.selectedObjectVertexIndex) = -1;
        (*context_.selectedObjectKind).clear();
        (*context_.selectedObjectCoordinate).reset();
        context_.clearInspectorObjectSelection();
        updateGeometrySelectionPresentation();
        context_.updateHelpPanel();
        context_.refreshObjectDetailsPanel();
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
    const bool usePendingClickSelection = (*context_.pendingClickSelection)
        && (*context_.pendingClickElapsed).isValid()
        && (*context_.pendingClickElapsed).elapsed() <= 1000;
    if (usePendingClickSelection && (*context_.pendingClickLineNumber) > 0) {
        if ((*context_.pendingClickSourceVertexIndex) >= 0) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item == nullptr) {
                    continue;
                }
                if (item->data(kMapSceneLineNumberRole).toInt() != (*context_.pendingClickLineNumber)) {
                    continue;
                }
                auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(item);
                if (vertexItem == nullptr) {
                    continue;
                }
                if (vertexItem->vertexIndex() != (*context_.pendingClickSourceVertexIndex)) {
                    continue;
                }
                if (!(*context_.pendingClickGeometryKind).isEmpty()
                    && !vertexItem->geometryKind().startsWith((*context_.pendingClickGeometryKind))) {
                    continue;
                }
                primarySelectedItem = item;
                break;
            }
        }
        if (primarySelectedItem == nullptr) {
            for (QGraphicsItem *item : selectedLineItems) {
                if (item != nullptr && item->data(kMapSceneLineNumberRole).toInt() == (*context_.pendingClickLineNumber)) {
                    primarySelectedItem = item;
                    break;
                }
            }
        }
    }
    if (context_.view != nullptr && context_.view->viewport() != nullptr) {
        if (primarySelectedItem == nullptr && usePendingClickSelection) {
            const QPointF scenePos = (*context_.pendingClickScenePosition);
            const QList<QGraphicsItem *> hitItems = context_.scene->items(scenePos,
                                                                     Qt::IntersectsItemShape,
                                                                     Qt::DescendingOrder,
                                                                     context_.view->transform());
            primarySelectedItem = preferredMapHitItem(hitItems, true, scenePos);
            if (primarySelectedItem == nullptr && (*context_.pendingClickLineNumber) > 0) {
                if (QGraphicsItem *pendingHitItem = preferredMapHitItem(hitItems, false, scenePos)) {
                    if (pendingHitItem->data(kMapSceneLineNumberRole).toInt() == (*context_.pendingClickLineNumber)) {
                        primarySelectedItem = pendingHitItem;
                    }
                }
            }
        } else if (primarySelectedItem == nullptr) {
            const QPoint viewportPos = context_.view->viewport()->mapFromGlobal(QCursor::pos());
            if (context_.view->viewport()->rect().contains(viewportPos)) {
                const QPointF scenePos = context_.view->mapToScene(viewportPos);
                const QList<QGraphicsItem *> hitItems = context_.scene->items(scenePos,
                                                                         Qt::IntersectsItemShape,
                                                                         Qt::DescendingOrder,
                                                                         context_.view->transform());
                primarySelectedItem = preferredMapHitItem(hitItems, true, scenePos);
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

    if (primarySelectedItem != nullptr && !primarySelectedItem->isSelected()) {
        const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
        context_.scene->clearSelection();
        primarySelectedItem->setSelected(true);
    }

    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();

    if (primarySelectedItem != nullptr) {
        selectedLineNumber = primarySelectedItem->data(kMapSceneLineNumberRole).toInt();
        selectedCard = dynamic_cast<MapCardItem *>(primarySelectedItem);
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem)) {
            selectedSourceVertexIndex = vertexItem->vertexIndex();
            selectedGeometryKind = vertexItem->geometryKind();
        } else if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(primarySelectedItem)) {
            selectedPointSource = pointItem->sourcePoint();
        } else if (primarySelectedItem->data(kMapSceneSelectionSubtypeRole).toInt() == kMapSceneSelectionSubtypePointOrientationHandle) {
            if (auto selectedItemIt = (*context_.itemsByLine).find(selectedLineNumber);
                selectedItemIt != (*context_.itemsByLine).end()) {
                if (auto *pointItem = dynamic_cast<MapEditablePointItem *>(selectedItemIt.value())) {
                    const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
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
    if (selectedLineNumber > 0 && context_.textEditor != nullptr) {
        const QScopedValueRollback<bool> syncGuard((*context_.textNavigationInProgress), true);
        if (selectedPointSource.has_value()) {
            const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(context_.textEditor->text());
            const std::optional<int> pointLineNumber =
                sourcePointLineNumberForSelection(parsedLines, selectedPointSource.value());
            if (pointLineNumber.has_value()) {
                selectedSourceLineNumber = pointLineNumber.value();
                context_.textEditor->goToLineColumn(pointLineNumber.value(), 1);
            } else if (selectedLineNumber != context_.currentLineNumber()) {
                context_.textEditor->goToLine(selectedLineNumber);
            }
        } else if (selectedSourceVertexIndex >= 0) {
            const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(context_.textEditor->text());
            const std::optional<SourceVertexTextReference> sourceReference =
                sourceVertexTextReferenceForSelection(parsedLines,
                                                      selectedLineNumber,
                                                      selectedGeometryKind,
                                                      selectedSourceVertexIndex);
            if (sourceReference.has_value()) {
                if (context_.textEditor->currentLineNumber() != sourceReference->lineNumber
                    || context_.textEditor->currentColumnNumber() != sourceReference->xStartColumn) {
                    context_.textEditor->goToLineColumn(sourceReference->lineNumber, sourceReference->xStartColumn);
                }
            } else if (selectedLineNumber != context_.currentLineNumber()) {
                context_.textEditor->goToLine(selectedLineNumber);
            }
        } else if (selectedLineNumber != context_.currentLineNumber()) {
            context_.textEditor->goToLine(selectedLineNumber);
        }
    }

    (*context_.selectedObjectLineNumber) = selectedSourceLineNumber;
    (*context_.selectedObjectVertexIndex) = selectedSourceVertexIndex;
    (*context_.selectedObjectCoordinate).reset();
    if (selectedPointSource.has_value()) {
        (*context_.selectedObjectKind) = QStringLiteral("point");
        (*context_.selectedObjectCoordinate) = selectedPointSource.value();
    } else if (selectedSourceVertexIndex >= 0) {
        const QString normalizedGeometryKind = selectedGeometryKind.trimmed().toLower();
        if (normalizedGeometryKind.startsWith(QStringLiteral("line"))) {
            (*context_.selectedObjectKind) = QStringLiteral("line");
        } else if (normalizedGeometryKind.startsWith(QStringLiteral("area"))) {
            (*context_.selectedObjectKind) = QStringLiteral("area");
        } else {
            (*context_.selectedObjectKind) = normalizedGeometryKind;
        }
        if (auto *vertexItem = dynamic_cast<MapEditableGeometryVertexItem *>(primarySelectedItem)) {
            (*context_.selectedObjectCoordinate) = context_.sourcePointFromScenePosition(vertexItem->pos());
        } else if (selectedLineNumber > 0 && selectedSourceVertexIndex >= 0 && context_.scene != nullptr) {
            if (MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(context_.scene,
                                                                                   selectedLineNumber,
                                                                                   selectedSourceVertexIndex,
                                                                                   QStringLiteral("line"))) {
                const QScopedValueRollback<bool> selectionGuard((*context_.updatingSelection), true);
                ownerAnchor->setSelected(true);
                (*context_.selectedObjectCoordinate) = context_.sourcePointFromScenePosition(ownerAnchor->pos());
            }
        }
    } else if (selectedLineNumber > 0 && context_.textEditor != nullptr) {
        if (const std::optional<MapGeometryFeature> feature = lineFeatureForLineNumber(context_.textEditor->text(), selectedLineNumber);
            feature.has_value()) {
            if (feature->kind == MapGeometryFeature::Kind::Line) {
                (*context_.selectedObjectKind) = QStringLiteral("line");
            } else if (feature->kind == MapGeometryFeature::Kind::Area) {
                (*context_.selectedObjectKind) = QStringLiteral("area");
            } else if (feature->kind == MapGeometryFeature::Kind::Point) {
                (*context_.selectedObjectKind) = QStringLiteral("point");
            }
        }
        if ((*context_.selectedObjectKind).isEmpty()) {
            (*context_.selectedObjectKind) = QStringLiteral("object");
        }
    } else {
        (*context_.selectedObjectKind).clear();
    }

    if (selectedCard != nullptr) {
        updateGeometrySelectionPresentation();
        context_.updateCommandSurfaceState();
        context_.updateHelpPanel();
        context_.refreshObjectDetailsPanel();
        return;
    }

    updateGeometrySelectionPresentation();
    context_.updateCommandSurfaceState();
    context_.updateHelpPanel();
    context_.refreshObjectDetailsPanel();
}

void MapEditorSelectionController::syncMapSelectionFromTextCursor(int lineNumber, int columnNumber)
{
    (*context_.lastCursorSyncedLine) = lineNumber;
    (*context_.lastCursorSyncedColumn) = columnNumber;

    if (context_.scene == nullptr || context_.textEditor == nullptr || lineNumber <= 0) {
        return;
    }
    if ((*context_.suppressedAutoReselectLineNumbers).contains(lineNumber)) {
        return;
    }
    if (!(*context_.suppressedAutoReselectLineNumbers).isEmpty()) {
        (*context_.suppressedAutoReselectLineNumbers).clear();
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(context_.textEditor->text());
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
    if (sourceVertexIndex < 0 || context_.scene == nullptr) {
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
        if (const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(context_.textEditor->text(),
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

        (*context_.updatingSelection) = true;
        if (ownerSourceVertexIndex >= 0) {
            if (MapEditableGeometryVertexItem *ownerAnchor = findGeometryVertexItem(context_.scene,
                                                                                     cursorSelection.featureLineNumber,
                                                                                     ownerSourceVertexIndex,
                                                                                     QStringLiteral("line"))) {
                ownerAnchor->setSelected(true);
            }
        }
        (*context_.updatingSelection) = false;
        updateGeometrySelectionPresentation();

        targetItem = findGeometryVertexItem(context_.scene,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("line"));
    } else {
        targetItem = findGeometryVertexItem(context_.scene,
                                            cursorSelection.featureLineNumber,
                                            sourceVertexIndex,
                                            QStringLiteral("area"));
    }

    if (targetItem != nullptr) {
        (*context_.updatingSelection) = true;
        targetItem->setSelected(true);
        (*context_.updatingSelection) = false;
        updateGeometrySelectionPresentation();
        context_.updateCommandSurfaceState();
        context_.updateHelpPanel();
    }
}

void MapEditorSelectionController::updateGeometrySelectionPresentation()
{
    if (context_.scene == nullptr) {
        return;
    }

    QSet<int> selectedLines;
    QHash<int, QSet<int>> selectedLineControlOwnersByLine;
    const QList<QGraphicsItem *> selectedItems = context_.scene->selectedItems();
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

    const auto sceneItems = context_.scene->items();
    for (QGraphicsItem *item : sceneItems) {
        if (item == nullptr) {
            continue;
        }
        if (!item->data(kMapSceneSelectionGatedRole).toBool()) {
            continue;
        }

        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt();
        bool visible = lineNumber > 0
            && !(*context_.hiddenObjectLines).contains(lineNumber)
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
    if (context_.scene == nullptr) {
        return;
    }

    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();
    (*context_.updatingSelection) = true;
    context_.scene->clearSelection();

    auto selectedItemIt = (*context_.itemsByLine).find(lineNumber);
    if (selectedItemIt != (*context_.itemsByLine).end() && selectedItemIt.value() != nullptr) {
        selectedItemIt.value()->setSelected(true);
        if (centerOnSelection && !(*context_.autoFitEnabled) && context_.view != nullptr) {
            context_.view->centerOn(selectedItemIt.value());
        }
    }

    (*context_.updatingSelection) = false;
    updateGeometrySelectionPresentation();
}

void MapEditorSelectionController::selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection)
{
    if (context_.scene == nullptr) {
        return;
    }

    (*context_.pendingClickSelection) = false;
    (*context_.pendingClickLineNumber) = 0;
    (*context_.pendingClickSourceVertexIndex) = -1;
    (*context_.pendingClickGeometryKind).clear();
    (*context_.updatingSelection) = true;
    context_.scene->clearSelection();

    QList<int> sortedLines = lineNumbers.values();
    std::sort(sortedLines.begin(), sortedLines.end());
    QGraphicsItem *firstSelectedItem = nullptr;
    for (const int lineNumber : std::as_const(sortedLines)) {
        auto selectedItemIt = (*context_.itemsByLine).find(lineNumber);
        if (selectedItemIt == (*context_.itemsByLine).end() || selectedItemIt.value() == nullptr) {
            continue;
        }
        selectedItemIt.value()->setSelected(true);
        if (firstSelectedItem == nullptr) {
            firstSelectedItem = selectedItemIt.value();
        }
    }

    if (centerOnSelection && firstSelectedItem != nullptr && !(*context_.autoFitEnabled) && context_.view != nullptr) {
        context_.view->centerOn(firstSelectedItem);
    }

    (*context_.updatingSelection) = false;
    updateGeometrySelectionPresentation();
}

}
