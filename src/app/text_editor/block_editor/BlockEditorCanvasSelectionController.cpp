#include "BlockEditorCanvasSelectionController.h"

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
        QGraphicsItem *blockItem = owner_->resolveBlockCanvasItem(item);
        if (blockItem == nullptr) {
            continue;
        }
        if (owner_->blockCanvasItemLineNumber(blockItem) == lineNumber) {
            owner_->selectBlockCanvasItem(blockItem, true);
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
        selectedBlock = owner_->resolveBlockCanvasItem(owner_->blockCanvasScene_->selectedItems().first());
    }
    if (selectedBlock == nullptr) {
        selectedBlock = owner_->resolveBlockCanvasItem(owner_->blockCanvasScene_->focusItem());
    }
    if (selectedBlock == nullptr) {
        owner_->clearBlockDetailsPane();
        return;
    }

    const int lineNumber = owner_->blockCanvasItemLineNumber(selectedBlock);
    const QString kind = owner_->blockCanvasItemKind(selectedBlock);
    if (!owner_->loadBlockDetailsForSelection(kind, lineNumber)) {
        owner_->blockDetailsSelectedLineNumber_ = lineNumber;
        owner_->blockDetailsSelectedKind_ = owner_->normalizedDirectiveToken(kind);
    }
}
}
