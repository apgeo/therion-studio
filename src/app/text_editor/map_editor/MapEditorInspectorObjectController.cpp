#include "MapEditorInspectorObjectController.h"

#include "../TextEditorTab.h"
#include "MapEditorInspectorData.h"
#include "MapEditorSceneSupport.h"
#include "../../../core/ProjectStructureIndex.h"
#include "../../../core/TherionDocumentParser.h"

#include <QAbstractItemView>
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

namespace TherionStudio
{
namespace
{
constexpr int kInspectorSourceLineRole = Qt::UserRole + 700;
constexpr int kInspectorSourceFileRole = Qt::UserRole + 701;
constexpr int kInspectorObjectNameColumn = 0;
constexpr int kInspectorObjectVisibilityColumn = 1;
constexpr int kInspectorObjectDeleteColumn = 2;
constexpr int kInspectorObjectColumnCount = 3;
}

MapEditorInspectorObjectController::MapEditorInspectorObjectController(MapEditorTab *owner)
    : owner_(owner)
{
}

MapEditorInspectorObjectController::MapEditorInspectorObjectController(const MapEditorTab *owner)
    : owner_(const_cast<MapEditorTab *>(owner))
{
}

void MapEditorInspectorObjectController::rebuildInspectorObjectsTree()
{
    if (owner_->mapObjectsModel_ == nullptr) {
        return;
    }

    owner_->inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
    owner_->inspectorObjectPressedWasSelected_ = false;
    owner_->mapObjectsModel_->clear();
    owner_->mapObjectsModel_->setColumnCount(kInspectorObjectColumnCount);
    owner_->mapObjectsModel_->setHorizontalHeaderLabels({owner_->tr("Objects"), QString(), QString()});
    configureInspectorObjectTreeColumns();

    const QString th2Path = owner_->filePath();
    if (owner_->textEditor_ == nullptr || !th2Path.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive)) {
        auto *placeholderItem = new QStandardItem(owner_->tr("Open a TH2 document to browse its objects by scrap"));
        placeholderItem->setEditable(false);
        owner_->mapObjectsModel_->appendRow({placeholderItem, new QStandardItem, new QStandardItem});
        return;
    }

    const QString currentText = owner_->textEditor_->text();
    const QVector<ProjectStructureEntry> entries = ProjectStructureIndex::scanTh2Objects(th2Path, currentText);
    if (entries.isEmpty()) {
        auto *placeholderItem = new QStandardItem(owner_->tr("No TH2 scraps, points, lines, or areas were found in the current document"));
        placeholderItem->setEditable(false);
        owner_->mapObjectsModel_->appendRow({placeholderItem, new QStandardItem, new QStandardItem});
        return;
    }

    QHash<int, TherionParsedLine> parsedLinesByLineNumber;
    for (const TherionParsedLine &parsedLine : TherionDocumentParser::parseText(currentText)) {
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
        if (entry.lineNumber > 0) {
            currentObjectLines.insert(entry.lineNumber);
        }

        auto *visibilityItem = new QStandardItem;
        visibilityItem->setEditable(false);
        visibilityItem->setTextAlignment(Qt::AlignCenter);
        visibilityItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        visibilityItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        if (entry.lineNumber > 0) {
            const bool visible = !owner_->hiddenInspectorObjectLines_.contains(entry.lineNumber);
            visibilityItem->setIcon(inspectorActionIcon(visible ? QStringLiteral("eye") : QStringLiteral("eye-off")));
            visibilityItem->setToolTip(visible ? owner_->tr("Hide object") : owner_->tr("Show object"));
        }

        auto *deleteItem = new QStandardItem;
        deleteItem->setEditable(false);
        deleteItem->setTextAlignment(Qt::AlignCenter);
        deleteItem->setData(entry.sourceFile, kInspectorSourceFileRole);
        deleteItem->setData(entry.lineNumber, kInspectorSourceLineRole);
        if (entry.lineNumber > 0) {
            deleteItem->setIcon(inspectorActionIcon(QStringLiteral("trash-2")));
            deleteItem->setToolTip(owner_->tr("Delete object from source"));
        }

        QStandardItem *parentItem = parentStack.isEmpty() ? owner_->mapObjectsModel_->invisibleRootItem() : parentStack.last();
        parentItem->appendRow({entryItem, visibilityItem, deleteItem});
        parentStack.append(entryItem);
    }
    for (auto it = owner_->hiddenInspectorObjectLines_.begin(); it != owner_->hiddenInspectorObjectLines_.end();) {
        if (!currentObjectLines.contains(*it)) {
            it = owner_->hiddenInspectorObjectLines_.erase(it);
        } else {
            ++it;
        }
    }

    if (owner_->mapObjectsTree_ != nullptr) {
        owner_->mapObjectsTree_->expandAll();
    }
    applyInspectorObjectVisibility();
    syncInspectorObjectSelectionToLine(owner_->currentLineNumber());
}

void MapEditorInspectorObjectController::configureInspectorObjectTreeColumns()
{
    if (owner_->mapObjectsTree_ == nullptr) {
        return;
    }

    if (owner_->mapObjectsModel_ != nullptr && owner_->mapObjectsModel_->columnCount() < kInspectorObjectColumnCount) {
        owner_->mapObjectsModel_->setColumnCount(kInspectorObjectColumnCount);
    }
    owner_->mapObjectsTree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (QHeaderView *header = owner_->mapObjectsTree_->header(); header != nullptr) {
        header->setStretchLastSection(false);
        header->setSectionsMovable(false);
        header->setMinimumSectionSize(22);
        header->setSectionResizeMode(kInspectorObjectNameColumn, QHeaderView::Stretch);
        header->setSectionResizeMode(kInspectorObjectVisibilityColumn, QHeaderView::Fixed);
        header->setSectionResizeMode(kInspectorObjectDeleteColumn, QHeaderView::Fixed);
        header->resizeSection(kInspectorObjectVisibilityColumn, 26);
        header->resizeSection(kInspectorObjectDeleteColumn, 26);
    }
}

QModelIndex MapEditorInspectorObjectController::findInspectorObjectIndexForLine(int lineNumber) const
{
    if (owner_->mapObjectsModel_ == nullptr || lineNumber <= 0) {
        return QModelIndex();
    }

    QModelIndex bestIndex;
    int bestLineNumber = -1;
    std::function<void(const QModelIndex &)> visitNode = [&](const QModelIndex &parentIndex) {
        const int rowCount = owner_->mapObjectsModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            const QModelIndex childIndex = owner_->mapObjectsModel_->index(row, 0, parentIndex);
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
    if (owner_->mapObjectsTree_ == nullptr || owner_->mapObjectsTree_->selectionModel() == nullptr || lineNumber <= 0) {
        return;
    }
    if (owner_->suppressedInspectorAutoReselectLineNumbers_.contains(lineNumber)) {
        return;
    }
    if (!owner_->suppressedInspectorAutoReselectLineNumbers_.isEmpty()) {
        owner_->suppressedInspectorAutoReselectLineNumbers_.clear();
    }

    const QModelIndex targetIndex = findInspectorObjectIndexForLine(lineNumber);
    if (!targetIndex.isValid()) {
        return;
    }

    owner_->lastInspectorClickedObjectLineNumber_ = 0;
    const QScopedValueRollback<bool> guard(owner_->updatingMapInspectorObjectSelection_, true);
    QSignalBlocker blocker(owner_->mapObjectsTree_->selectionModel());
    setInspectorObjectCurrentIndex(targetIndex);
    if (scrollToSelection) {
        owner_->mapObjectsTree_->scrollTo(targetIndex, QAbstractItemView::EnsureVisible);
    }
}

void MapEditorInspectorObjectController::setInspectorObjectCurrentIndex(const QModelIndex &index)
{
    if (!index.isValid() || owner_->mapObjectsTree_ == nullptr || owner_->mapObjectsTree_->selectionModel() == nullptr) {
        return;
    }

    QItemSelectionModel *selectionModel = owner_->mapObjectsTree_->selectionModel();
    selectionModel->clear();
    owner_->mapObjectsTree_->setCurrentIndex(QModelIndex());
    selectionModel->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    selectionModel->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    if (QWidget *viewport = owner_->mapObjectsTree_->viewport(); viewport != nullptr) {
        viewport->update();
    }
}

void MapEditorInspectorObjectController::clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers)
{
    owner_->inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
    owner_->inspectorObjectPressedWasSelected_ = false;
    owner_->lastInspectorClickedObjectLineNumber_ = 0;
    owner_->suppressedInspectorAutoReselectLineNumbers_ = suppressAutoReselectLineNumbers;
    if (owner_->mapObjectsTree_ != nullptr && owner_->mapObjectsTree_->selectionModel() != nullptr) {
        const QScopedValueRollback<bool> guard(owner_->updatingMapInspectorObjectSelection_, true);
        QSignalBlocker blocker(owner_->mapObjectsTree_->selectionModel());
        owner_->mapObjectsTree_->selectionModel()->clearSelection();
        owner_->mapObjectsTree_->setCurrentIndex(QModelIndex());
    }

    if (owner_->mapScene_ != nullptr) {
        owner_->mapScene_->clearSelection();
        return;
    }

    owner_->selectedObjectLineNumber_ = 0;
    owner_->selectedObjectVertexIndex_ = -1;
    owner_->selectedObjectKind_.clear();
    owner_->selectedObjectCoordinate_.reset();
    owner_->refreshObjectDetailsPanel();
    owner_->updateCommandSurfaceState();
}

void MapEditorInspectorObjectController::handleInspectorObjectSelectionChanged(const QModelIndex &current)
{
    if (owner_->updatingMapInspectorObjectSelection_ || !current.isValid() || owner_->textEditor_ == nullptr) {
        return;
    }

    const QModelIndex objectIndex = current.sibling(current.row(), kInspectorObjectNameColumn);
    const int lineNumber = objectIndex.data(kInspectorSourceLineRole).toInt();
    if (lineNumber > 0) {
        owner_->textEditor_->goToLine(lineNumber);
    }
}

void MapEditorInspectorObjectController::handleInspectorObjectClicked(const QModelIndex &index)
{
    if (!index.isValid() || owner_->mapObjectsModel_ == nullptr || owner_->textEditor_ == nullptr) {
        return;
    }

    if (index.column() != kInspectorObjectNameColumn
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
        const bool repeatedClickOnSelectedRow = owner_->inspectorObjectPressedWasSelected_
            && owner_->inspectorObjectPressedNameIndex_.isValid()
            && owner_->inspectorObjectPressedNameIndex_ == objectIndex;
        owner_->inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
        owner_->inspectorObjectPressedWasSelected_ = false;
        if (repeatedClickOnSelectedRow) {
            QSet<int> suppressedLines;
            std::function<void(const QModelIndex &)> collectSuppressedLines = [&](const QModelIndex &parentIndex) {
                const int sourceLine = parentIndex.data(kInspectorSourceLineRole).toInt();
                if (sourceLine > 0) {
                    suppressedLines.insert(sourceLine);
                }

                const int rowCount = owner_->mapObjectsModel_->rowCount(parentIndex);
                for (int row = 0; row < rowCount; ++row) {
                    collectSuppressedLines(owner_->mapObjectsModel_->index(row, kInspectorObjectNameColumn, parentIndex));
                }
            };
            collectSuppressedLines(objectIndex);
            clearInspectorObjectSelection(suppressedLines);
            return;
        }

        owner_->suppressedInspectorAutoReselectLineNumbers_.clear();
        if (owner_->mapObjectsTree_ != nullptr && owner_->mapObjectsTree_->selectionModel() != nullptr) {
            setInspectorObjectCurrentIndex(objectIndex);
        }
        owner_->textEditor_->goToLine(lineNumber);
        owner_->syncMapSelectionFromTextCursor(lineNumber, owner_->textEditor_->currentColumnNumber());
        owner_->lastInspectorClickedObjectLineNumber_ = lineNumber;
        return;
    }

    owner_->inspectorObjectPressedNameIndex_ = QPersistentModelIndex();
    owner_->inspectorObjectPressedWasSelected_ = false;
    owner_->suppressedInspectorAutoReselectLineNumbers_.clear();
    if (index.column() == kInspectorObjectDeleteColumn) {
        if (owner_->textEditor_->deleteCommandAtLine(lineNumber)) {
            owner_->hiddenInspectorObjectLines_.remove(lineNumber);
            owner_->lastInspectorClickedObjectLineNumber_ = 0;
        }
        return;
    }

    owner_->lastInspectorClickedObjectLineNumber_ = 0;
    QVector<int> subtreeLineNumbers;
    std::function<void(const QModelIndex &)> collectLineNumbers = [&](const QModelIndex &parentIndex) {
        const int sourceLine = parentIndex.data(kInspectorSourceLineRole).toInt();
        if (sourceLine > 0) {
            subtreeLineNumbers.append(sourceLine);
        }

        const int rowCount = owner_->mapObjectsModel_->rowCount(parentIndex);
        for (int row = 0; row < rowCount; ++row) {
            collectLineNumbers(owner_->mapObjectsModel_->index(row, kInspectorObjectNameColumn, parentIndex));
        }
    };
    collectLineNumbers(objectIndex);

    if (subtreeLineNumbers.isEmpty()) {
        return;
    }

    const bool shouldShow = owner_->hiddenInspectorObjectLines_.contains(lineNumber);
    for (int sourceLine : subtreeLineNumbers) {
        if (shouldShow) {
            owner_->hiddenInspectorObjectLines_.remove(sourceLine);
        } else {
            owner_->hiddenInspectorObjectLines_.insert(sourceLine);
        }
    }

    rebuildInspectorObjectsTree();
    applyInspectorObjectVisibility();
    syncInspectorObjectSelectionToLine(lineNumber, false);
    if (shouldShow) {
        owner_->selectMapLines(QSet<int>(subtreeLineNumbers.begin(), subtreeLineNumbers.end()), false);
    }
}

void MapEditorInspectorObjectController::applyInspectorObjectVisibility()
{
    if (owner_->mapScene_ == nullptr) {
        return;
    }

    for (QGraphicsItem *item : owner_->mapScene_->items()) {
        if (item == nullptr) {
            continue;
        }

        bool ok = false;
        const int lineNumber = item->data(kMapSceneLineNumberRole).toInt(&ok);
        if (!ok || lineNumber <= 0) {
            continue;
        }

        const bool hidden = owner_->hiddenInspectorObjectLines_.contains(lineNumber);
        if (item->data(kMapSceneSelectionGatedRole).toBool()) {
            if (hidden) {
                item->setVisible(false);
            }
            continue;
        }

        item->setVisible(!hidden);
    }

    owner_->updateGeometrySelectionPresentation();
}
}
