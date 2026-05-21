#include "BlockEditorCanvasSelectionController.h"

#include "BlockEditorCanvasItem.h"
#include "../TextEditorTab.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace TherionStudio
{
BlockEditorCanvasSelectionController::BlockEditorCanvasSelectionController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorCanvasSelectionController::selectBlockInCanvasAndDetails(int lineNumber)
{
    if (owner_ == nullptr || owner_->tearingDown_) {
        return;
    }
    if (lineNumber <= 0 || owner_->blockCanvasScene_ == nullptr) {
        owner_->clearBlockDetailsPane();
        return;
    }

    const QList<QGraphicsItem *> sceneItems = owner_->blockCanvasScene_->items();
    for (QGraphicsItem *item : sceneItems) {
        QGraphicsItem *blockItem = resolveBlockCanvasItem(item);
        if (blockItem == nullptr) {
            continue;
        }
        if (blockCanvasItemLineNumber(blockItem) == lineNumber) {
            selectBlockCanvasItem(blockItem, true);
            owner_->refreshBlockDetailsSelectionFromScene();
            return;
        }
    }
    owner_->clearBlockDetailsPane();
}

void BlockEditorCanvasSelectionController::refreshDetailsSelectionFromScene()
{
    if (owner_ == nullptr || owner_->tearingDown_) {
        return;
    }
    if (owner_->blockCanvasScene_ == nullptr) {
        owner_->clearBlockDetailsPane();
        return;
    }

    QGraphicsItem *selectedBlock = nullptr;
    if (!owner_->blockCanvasScene_->selectedItems().isEmpty()) {
        selectedBlock = resolveBlockCanvasItem(owner_->blockCanvasScene_->selectedItems().first());
    }
    if (selectedBlock == nullptr) {
        selectedBlock = resolveBlockCanvasItem(owner_->blockCanvasScene_->focusItem());
    }
    if (selectedBlock == nullptr) {
        owner_->clearBlockDetailsPane();
        return;
    }

    const int lineNumber = blockCanvasItemLineNumber(selectedBlock);
    const QString kind = blockCanvasItemKind(selectedBlock);
    if (!owner_->loadBlockDetailsForSelection(kind, lineNumber)) {
        owner_->blockDetailsSelectedLineNumber_ = lineNumber;
        owner_->blockDetailsSelectedKind_ = owner_->normalizedDirectiveToken(kind);
    }
}

QGraphicsItem *BlockEditorCanvasSelectionController::resolveBlockCanvasItem(QGraphicsItem *item) const
{
    while (item != nullptr) {
        if (dynamic_cast<BlockCanvasItem *>(item) != nullptr) {
            return item;
        }
        item = item->parentItem();
    }
    return nullptr;
}

int BlockEditorCanvasSelectionController::blockCanvasItemLineNumber(const QGraphicsItem *item) const
{
    if (const auto *blockItem = dynamic_cast<const BlockCanvasItem *>(item); blockItem != nullptr) {
        return blockItem->lineNumber();
    }
    return 0;
}

QString BlockEditorCanvasSelectionController::blockCanvasItemKind(const QGraphicsItem *item) const
{
    if (const auto *blockItem = dynamic_cast<const BlockCanvasItem *>(item); blockItem != nullptr) {
        return blockItem->kind();
    }
    return QString();
}

void BlockEditorCanvasSelectionController::selectBlockCanvasItem(QGraphicsItem *item, bool centerView)
{
    if (owner_ == nullptr || owner_->blockCanvasScene_ == nullptr || item == nullptr) {
        return;
    }
    owner_->blockCanvasScene_->clearSelection();
    item->setSelected(true);
    owner_->blockCanvasScene_->setFocusItem(item);
    if (centerView && owner_->blockCanvasView_ != nullptr) {
        owner_->blockCanvasView_->centerOn(item);
    }
}
}
