#include "MapEditorInspectorObjectController.h"

#include "../TextEditorTab.h"
#include "MapEditorAreaReferenceResolver.h"
#include "MapEditorInspectorData.h"
#include "MapEditorObjectDeletePlanner.h"
#include "MapEditorObjectMovePlanner.h"
#include "MapEditorSceneSupport.h"
#include "../../../core/ProjectStructureIndex.h"
#include "../../../core/TherionDocumentParser.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTreeView>

#include <functional>
#include <utility>

namespace TherionStudio
{
namespace
{
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;
constexpr int kInspectorSourceFileRole = Qt::UserRole + 701;
constexpr int kInspectorObjectCategoryRole = Qt::UserRole + 702;
constexpr int kInspectorObjectDeleteBlockedRole = Qt::UserRole + 703;
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectDragColumn = 1;
constexpr int kInspectorObjectVisibilityColumn = 2;
constexpr int kInspectorObjectDeleteColumn = 3;
constexpr int kInspectorObjectColumnCount = 4;

bool isScrapCategory(const QString &category)
{
    return category == QStringLiteral("Scraps");
}

bool requireSourceTransaction(const MapEditorInspectorObjectContext &context, const QString &message)
{
    if (context.applySourceTextChangeWithSnapshot) {
        return true;
    }

    Q_ASSERT(context.applySourceTextChangeWithSnapshot);
    if (context.toolbarStatusNote != nullptr) {
        *context.toolbarStatusNote = message;
    }
    if (context.refreshToolbarSummary) {
        context.refreshToolbarSummary();
    }
    if (context.refreshObjectDetailsPanel) {
        context.refreshObjectDetailsPanel();
    }
    return false;
}
}

MapEditorInspectorObjectController::MapEditorInspectorObjectController(MapEditorInspectorObjectContext context)
    : context_(std::move(context))
{
}

QString MapEditorInspectorObjectController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::MapEditorInspectorObjectController", text);
}

void MapEditorInspectorObjectController::rebuildInspectorObjectsTree()
{
    if (context_.objectsModel == nullptr) {
        return;
    }

    *context_.pressedNameIndex = QPersistentModelIndex();
    *context_.pressedWasSelected = false;
    context_.objectsModel->clear();
    context_.objectsModel->setColumnCount(kInspectorObjectColumnCount);
    context_.objectsModel->setHorizontalHeaderLabels({tr("Objects"), QString(), QString(), QString()});
    configureInspectorObjectTreeColumns();

    const QString th2Path = context_.filePath();
    if (context_.textEditor == nullptr || !th2Path.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        auto *placeholderItem = new QStandardItem(tr("Open a TH2 document to browse its objects by scrap"));
        placeholderItem->setEditable(false);
        context_.objectsModel->appendRow({placeholderItem, new QStandardItem, new QStandardItem});
        return;
    }

    const QString currentText = context_.textEditor->text();
    const QVector<TherionParsedLine> parsedLines = context_.parsedLinesForCurrentDocument();
    const QVector<ProjectStructureEntry> entries = ProjectStructureIndex::scanTh2Objects(th2Path, parsedLines);
    if (entries.isEmpty()) {
        auto *placeholderItem = new QStandardItem(tr("No TH2 scraps, points, lines, or areas were found in the current document"));
        placeholderItem->setEditable(false);
        context_.objectsModel->appendRow({placeholderItem, new QStandardItem, new QStandardItem});
        return;
    }

    QHash<int, TherionParsedLine> parsedLinesByLineNumber;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.lineNumber > 0) {
            parsedLinesByLineNumber.insert(parsedLine.lineNumber, parsedLine);
        }
    }

    QSet<int> currentObjectLines;
    QVector<QStandardItem *> parentStack;
    for (const ProjectStructureEntry &entry : entries) {
        while (parentStack.size() > entry.depth) {
            parentStack.removeLast();
        }

        const auto parsedLineIt = parsedLinesByLineNumber.constFind(entry.lineNumber);
        const TherionParsedLine *parsedLine = parsedLineIt == parsedLinesByLineNumber.constEnd()
            ? nullptr
            : &parsedLineIt.value();
        auto *entryItem = new QStandardItem(inspectorMapObjectItemText(entry, parsedLine));
        const QString iconName = inspectorMapObjectIconName(entry);
        if (!iconName.isEmpty()) {
            entryItem->setIcon(inspectorActionIcon(iconName));
        }
        entryItem->setEditable(false);
        entryItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        entryItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        entryItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0) {
            currentObjectLines.insert(entry.lineNumber);
        }

        auto *dragItem = new QStandardItem;
        dragItem->setEditable(false);
        dragItem->setTextAlignment(Qt::AlignCenter);
        dragItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        dragItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        dragItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0 && !isScrapCategory(entry.category)) {
            dragItem->setIcon(inspectorActionIcon(QStringLiteral("grip-vertical")));
            dragItem->setToolTip(tr("Drag to move this object"));
        }

        auto *visibilityItem = new QStandardItem;
        visibilityItem->setEditable(false);
        visibilityItem->setTextAlignment(Qt::AlignCenter);
        visibilityItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        visibilityItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        visibilityItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0) {
            const bool visible = !context_.hiddenObjectLines->contains(entry.lineNumber);
            visibilityItem->setIcon(inspectorActionIcon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off")));
            visibilityItem->setToolTip(visible ? tr("Hide object") : tr("Show object"));
        }

        auto *deleteItem = new QStandardItem;
        deleteItem->setEditable(false);
        deleteItem->setTextAlignment(Qt::AlignCenter);
        deleteItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        deleteItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        deleteItem->setData(entry.category, kInspectorObjectCategoryRole);
        if (entry.lineNumber > 0) {
            const QVector<MapEditorAreaReference> areaReferences =
                mapEditorAreaReferencesForBorderLine(currentText, entry.lineNumber);
            const bool deleteBlockedByAreaReference = !areaReferences.isEmpty();
            deleteItem->setIcon(inspectorActionIcon(QStringLiteral("trash-2")));
            deleteItem->setData(deleteBlockedByAreaReference, kInspectorObjectDeleteBlockedRole);
            deleteItem->setEnabled(!deleteBlockedByAreaReference);
            deleteItem->setToolTip(deleteBlockedByAreaReference
                ? tr("This line is used as an area border. Delete the area instead.")
                : tr("Delete object from source"));
        }

        QStandardItem *parentItem = parentStack.isEmpty() ? context_.objectsModel->invisibleRootItem() : parentStack.last();
        parentItem->appendRow({entryItem, dragItem, visibilityItem, deleteItem});
        parentStack.append(entryItem);
    }
    for (auto it = context_.hiddenObjectLines->begin(); it != context_.hiddenObjectLines->end();) {
        if (!currentObjectLines.contains(*it)) {
            it = context_.hiddenObjectLines->erase(it);
        } else {
            ++it;
        }
    }

    if (context_.objectsTree != nullptr) {
        context_.objectsTree->expandAll();
    }
    applyInspectorObjectVisibility();
    syncInspectorObjectSelectionToLine(context_.currentLineNumber());
}

void MapEditorInspectorObjectController::configureInspectorObjectTreeColumns()
{
    if (context_.objectsTree == nullptr) {
        return;
    }

    if (context_.objectsModel != nullptr && context_.objectsModel->columnCount() < kInspectorObjectColumnCount) {
        context_.objectsModel->setColumnCount(kInspectorObjectColumnCount);
    }
    context_.objectsTree->setMouseTracking(true);
    if (context_.objectsTree->viewport() != nullptr) {
        context_.objectsTree->viewport()->setMouseTracking(true);
    }
    context_.objectsTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (QHeaderView *header = context_.objectsTree->header(); header != nullptr) {
        header->setStretchLastSection(false);
        header->setSectionsMovable(false);
        header->setMinimumSectionSize(22);
        header->setSectionResizeMode(kInspectorObjectNameColumn, QHeaderView::Stretch);
        header->setSectionResizeMode(kInspectorObjectDragColumn, QHeaderView::Fixed);
        header->setSectionResizeMode(kInspectorObjectVisibilityColumn, QHeaderView::Fixed);
        header->setSectionResizeMode(kInspectorObjectDeleteColumn, QHeaderView::Fixed);
        header->resizeSection(kInspectorObjectDragColumn, 24);
        header->resizeSection(kInspectorObjectVisibilityColumn, 26);
        header->resizeSection(kInspectorObjectDeleteColumn, 26);
    }
}

QModelIndex MapEditorInspectorObjectController::findInspectorObjectIndexForLine(int lineNumber) const
{
    if (context_.objectsModel == nullptr || lineNumber <= 0) {
        return QModelIndex();
    }

    QModelIndex bestIndex;
    int bestLineNumber = -1;
    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = context_.objectsModel->index(row, 0, parentIndex);
            if (!childIndex.isValid()) {
                continue;
            }

            const int candidateLineNumber = childIndex.data(kInspectorSourceLineRole).toInt();
            if (candidateLineNumber > 0 && candidateLineNumber <= lineNumber && candidateLineNumber >= bestLineNumber) {
                bestIndex = childIndex;
                bestLineNumber = candidateLineNumber;
            }

            visitNode(childIndex);
        }
    };
    visitNode(QModelIndex());
    return bestIndex;
}

void MapEditorInspectorObjectController::syncInspectorObjectSelectionToLine(int lineNumber)
{
    syncInspectorObjectSelectionToLine(lineNumber, true);
}

void MapEditorInspectorObjectController::syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection)
{
    if (context_.objectsTree == nullptr || context_.objectsTree->selectionModel() == nullptr || lineNumber <= 0) {
        return;
    }
    if (context_.suppressedAutoReselectLineNumbers->contains(lineNumber)) {
        return;
    }
    if (!context_.suppressedAutoReselectLineNumbers->isEmpty()) {
        context_.suppressedAutoReselectLineNumbers->clear();
    }

    const QModelIndex targetIndex = findInspectorObjectIndexForLine(lineNumber);
    if (!targetIndex.isValid()) {
        return;
    }

    *context_.lastClickedLineNumber = 0;
    const QScopedValueRollback<bool> guard(*context_.updatingSelection, true);
    QSignalBlocker blocker(context_.objectsTree->selectionModel());
    setInspectorObjectCurrentIndex(targetIndex);
    if (scrollToSelection) {
        context_.objectsTree->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
    }
}

void MapEditorInspectorObjectController::setInspectorObjectCurrentIndex(const QModelIndex &index)
{
    if (!index.isValid() || context_.objectsTree == nullptr || context_.objectsTree->selectionModel() == nullptr) {
        return;
    }

    QItemSelectionModel *selectionModel = context_.objectsTree->selectionModel();
    selectionModel->clear();
    context_.objectsTree->setCurrentIndex(QModelIndex());
    selectionModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    if (QWidget *viewport = context_.objectsTree->viewport(); viewport != nullptr) {
        viewport->update();
    }
}

void MapEditorInspectorObjectController::clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers)
{
    *context_.pressedNameIndex = QPersistentModelIndex();
    *context_.pressedWasSelected = false;
    *context_.lastClickedLineNumber = 0;
    *context_.suppressedAutoReselectLineNumbers = suppressAutoReselectLineNumbers;
    if (context_.objectsTree != nullptr && context_.objectsTree->selectionModel() != nullptr) {
        const QScopedValueRollback<bool> guard(*context_.updatingSelection, true);
        QSignalBlocker blocker(context_.objectsTree->selectionModel());
        context_.objectsTree->selectionModel()->clearSelection();
        context_.objectsTree->setCurrentIndex(QModelIndex());
    }

    if (context_.scene != nullptr) {
        context_.scene->clearSelection();
        return;
    }

    *context_.selectedObjectLineNumber = 0;
    *context_.selectedObjectVertexIndex = -1;
    context_.selectedObjectKind->clear();
    context_.selectedObjectCoordinate->reset();
    context_.refreshObjectDetailsPanel();
    context_.updateCommandSurfaceState();
}

void MapEditorInspectorObjectController::handleInspectorObjectSelectionChanged(const QModelIndex &current)
{
    if (*context_.updatingSelection || !current.isValid() || context_.textEditor == nullptr) {
        return;
    }

    const QModelIndex objectIndex = current.sibling(current.row(), kInspectorObjectNameColumn);
    const int lineNumber = objectIndex.data(kInspectorSourceLineRole).toInt();
    if (lineNumber > 0) {
        context_.textEditor->goToLine(lineNumber);
    }
}

void MapEditorInspectorObjectController::handleInspectorObjectClicked(const QModelIndex &index)
{
    if (!index.isValid() || context_.objectsModel == nullptr || context_.textEditor == nullptr) {
        return;
    }

    if (index.column() != kInspectorObjectNameColumn
        && index.column() != kInspectorObjectDragColumn
        && index.column() != kInspectorObjectVisibilityColumn
        && index.column() != kInspectorObjectDeleteColumn) {
        return;
    }

    const QModelIndex objectIndex = index.sibling(index.row(), kInspectorObjectNameColumn);
    const int lineNumber = objectIndex.data(kInspectorSourceLineRole).toInt();
    if (lineNumber <= 0) {
        return;
    }

    if (index.column() == kInspectorObjectNameColumn) {
        const bool repeatedClickOnSelectedRow = *context_.pressedWasSelected
            && context_.pressedNameIndex->isValid()
            && *context_.pressedNameIndex == objectIndex;
        *context_.pressedNameIndex = QPersistentModelIndex();
        *context_.pressedWasSelected = false;
        if (repeatedClickOnSelectedRow) {
            QSet<int> suppressedLines;
            std::function<void(const QModelIndex &)> collectSuppressedLines = [&](const QModelIndex &parentIndex) {
                const int sourceLine = parentIndex.data(kInspectorSourceLineRole).toInt();
                if (sourceLine > 0) {
                    suppressedLines.insert(sourceLine);
                }

                const int rowCount = context_.objectsModel->rowCount(parentIndex);
                for (int row = 0; row < rowCount; ++row) {
                    collectSuppressedLines(context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex));
                }
            };
            collectSuppressedLines(objectIndex);
            clearInspectorObjectSelection(suppressedLines);
            return;
        }

        context_.suppressedAutoReselectLineNumbers->clear();
        if (context_.objectsTree != nullptr && context_.objectsTree->selectionModel() != nullptr) {
            setInspectorObjectCurrentIndex(objectIndex);
        }
        context_.textEditor->goToLine(lineNumber);
        context_.syncMapSelectionFromTextCursor(lineNumber, context_.textEditor->currentColumnNumber());
        *context_.lastClickedLineNumber = lineNumber;
        return;
    }

    if (index.column() == kInspectorObjectDragColumn) {
        return;
    }

    *context_.pressedNameIndex = QPersistentModelIndex();
    *context_.pressedWasSelected = false;
    context_.suppressedAutoReselectLineNumbers->clear();
    if (index.column() == kInspectorObjectDeleteColumn) {
        if (index.data(kInspectorObjectDeleteBlockedRole).toBool()) {
            if (context_.toolbarStatusNote != nullptr) {
                *context_.toolbarStatusNote = tr("This line is used as an area border. Delete the area instead.");
            }
            if (context_.refreshToolbarSummary) {
                context_.refreshToolbarSummary();
            }
            return;
        }
        const QString beforeText = context_.textEditor->text();
        const MapEditorObjectDeletePlan deletePlan = planMapEditorObjectDelete(beforeText, lineNumber);
        if (!deletePlan.resolved || !deletePlan.changed) {
            if (context_.toolbarStatusNote != nullptr) {
                *context_.toolbarStatusNote = deletePlan.errorMessage.isEmpty()
                    ? tr("Object deletion failed.")
                    : tr("Object deletion failed: %1").arg(deletePlan.errorMessage);
            }
            if (context_.refreshToolbarSummary) {
                context_.refreshToolbarSummary();
            }
            return;
        }

        if (!requireSourceTransaction(context_, tr("Cannot delete map object without map source transaction support."))) {
            return;
        }
        const auto postDeleteHook = [this, deletePlan]() {
            for (int removedLineNumber : deletePlan.removedLineNumbers) {
                context_.hiddenObjectLines->remove(removedLineNumber);
            }
            *context_.lastClickedLineNumber = 0;
        };

        context_.applySourceTextChangeWithSnapshot(tr("Delete Map Object"),
                                                   beforeText,
                                                   deletePlan.updatedText,
                                                   deletePlan.focusLineAfterDelete,
                                                   postDeleteHook);

        if (context_.toolbarStatusNote != nullptr) {
            *context_.toolbarStatusNote = tr("Deleted selected object from source.");
        }
        if (context_.refreshToolbarSummary) {
            context_.refreshToolbarSummary();
        }
        return;
    }

    *context_.lastClickedLineNumber = 0;
    QVector<int> subtreeLineNumbers;
    std::function<void(const QModelIndex &)> collectLineNumbers = [&](const QModelIndex &parentIndex) {
        const int sourceLine = parentIndex.data(kInspectorSourceLineRole).toInt();
        if (sourceLine > 0) {
            subtreeLineNumbers.append(sourceLine);
        }

        const int rowCount = context_.objectsModel->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            collectLineNumbers(context_.objectsModel->index(row, kInspectorObjectNameColumn, parentIndex));
        }
    };
    collectLineNumbers(objectIndex);

    if (subtreeLineNumbers.isEmpty()) {
        return;
    }

    const bool shouldShow = context_.hiddenObjectLines->contains(lineNumber);
    for (int sourceLine : subtreeLineNumbers) {
        if (shouldShow) {
            context_.hiddenObjectLines->remove(sourceLine);
        } else {
            context_.hiddenObjectLines->insert(sourceLine);
        }
    }

    rebuildInspectorObjectsTree();
    applyInspectorObjectVisibility();
    syncInspectorObjectSelectionToLine(lineNumber, false);
    if (shouldShow) {
        context_.selectMapLines(QSet<int>(subtreeLineNumbers.begin(), subtreeLineNumbers.end()), false);
    }
}

bool MapEditorInspectorObjectController::moveInspectorObject(const QModelIndex &sourceIndex, const QModelIndex &targetIndex, bool afterTarget)
{
    if (!sourceIndex.isValid() || !targetIndex.isValid() || context_.textEditor == nullptr) {
        return false;
    }

    const QModelIndex sourceObjectIndex = sourceIndex.sibling(sourceIndex.row(), kInspectorObjectNameColumn);
    const QModelIndex targetObjectIndex = targetIndex.sibling(targetIndex.row(), kInspectorObjectNameColumn);
    const int sourceLineNumber = sourceObjectIndex.data(kInspectorSourceLineRole).toInt();
    const int targetLineNumber = targetObjectIndex.data(kInspectorSourceLineRole).toInt();
    if (sourceLineNumber <= 0 || targetLineNumber <= 0 || sourceLineNumber == targetLineNumber) {
        return false;
    }

    const bool targetIsScrap = isScrapCategory(targetObjectIndex.data(kInspectorObjectCategoryRole).toString());
    const QString beforeText = context_.textEditor->text();
    const MapEditorObjectMovePlan movePlan = MapEditorObjectMovePlanner::planMove(beforeText,
                                                                                  sourceLineNumber,
                                                                                  targetLineNumber,
                                                                                  targetIsScrap
                                                                                      ? MapEditorObjectMovePosition::IntoTargetScrap
                                                                                      : (afterTarget
                                                                                             ? MapEditorObjectMovePosition::AfterTarget
                                                                                             : MapEditorObjectMovePosition::BeforeTarget));
    if (!movePlan.resolved) {
        if (context_.toolbarStatusNote != nullptr) {
            *context_.toolbarStatusNote = movePlan.errorMessage.isEmpty()
                ? tr("Object move failed.")
                : tr("Object move failed: %1").arg(movePlan.errorMessage);
        }
        if (context_.refreshToolbarSummary) {
            context_.refreshToolbarSummary();
        }
        return false;
    }

    if (!movePlan.changed) {
        if (context_.toolbarStatusNote != nullptr) {
            *context_.toolbarStatusNote = tr("Object already at requested position.");
        }
        if (context_.refreshToolbarSummary) {
            context_.refreshToolbarSummary();
        }
        return false;
    }

    if (!requireSourceTransaction(context_, tr("Cannot move map object without map source transaction support."))) {
        return false;
    }
    const auto postMoveHook = [this, movePlan]() {
        context_.hiddenObjectLines->clear();
        context_.textEditor->goToLine(movePlan.insertBeforeLineAfterRemoval);
        context_.syncMapSelectionFromTextCursor(movePlan.insertBeforeLineAfterRemoval,
                                                context_.textEditor->currentColumnNumber());
        rebuildInspectorObjectsTree();
        syncInspectorObjectSelectionToLine(movePlan.insertBeforeLineAfterRemoval, true);
    };

    context_.applySourceTextChangeWithSnapshot(tr("Move Map Object"),
                                               beforeText,
                                               movePlan.movedText,
                                               movePlan.insertBeforeLineAfterRemoval,
                                               postMoveHook);

    if (context_.toolbarStatusNote != nullptr) {
        *context_.toolbarStatusNote = tr("Moved map object to line %1.").arg(movePlan.insertBeforeLineAfterRemoval);
    }
    if (context_.refreshToolbarSummary) {
        context_.refreshToolbarSummary();
    }
    return true;
}

void MapEditorInspectorObjectController::applyInspectorObjectVisibility()
{
    if (context_.scene == nullptr) {
        return;
    }

    for (QGraphicsItem *item : context_.scene->items()) {
        if (item == nullptr) {
            continue;
        }

        bool ok = false;
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt(&ok);
        if (!ok || lineNumber <= 0) {
            continue;
        }

        const bool hidden = context_.hiddenObjectLines->contains(lineNumber);
        if (item->data(kMapSceneSelectionGatedRole).toBool()) {
            if (hidden) {
                item->setVisible(false);
            }
            continue;
        }

        item->setVisible(!hidden);
    }

    context_.updateGeometrySelectionPresentation();
}
}
