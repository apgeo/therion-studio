#include "BlockEditorCanvasSelectionController.h"

#include "BlockEditorCanvasItem.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

#include <utility>

namespace TherionStudio
{
BlockEditorCanvasSelectionController::BlockEditorCanvasSelectionController(BlockEditorCanvasSelectionContext context)
    : context_(std::move(context))
{
}

void BlockEditorCanvasSelectionController::selectBlockInCanvasAndDetails(int lineNumber)
{
    if (context_.tearingDown != nullptr && *context_.tearingDown) {
        return;
    }
    if (lineNumber <= 0 || context_.scene == nullptr) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return;
    }

    const QList<QGraphicsItem *> sceneItems = context_.scene->items();
    for (QGraphicsItem *item : sceneItems) {
        QGraphicsItem *blockItem = resolveBlockCanvasItem(item);
        if (blockItem == nullptr) {
            continue;
        }
        if (blockCanvasItemLineNumber(blockItem) == lineNumber) {
            selectBlockCanvasItem(blockItem, true);
            refreshDetailsSelectionFromScene();
            return;
        }
    }
    if (context_.clearDetailsPane) {
        context_.clearDetailsPane();
    }
}

void BlockEditorCanvasSelectionController::refreshDetailsSelectionFromScene()
{
    if (context_.tearingDown != nullptr && *context_.tearingDown) {
        return;
    }
    if (context_.scene == nullptr) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return;
    }

    QGraphicsItem *selectedBlock = nullptr;
    if (!context_.scene->selectedItems().isEmpty()) {
        selectedBlock = resolveBlockCanvasItem(context_.scene->selectedItems().first());
    }
    if (selectedBlock == nullptr) {
        selectedBlock = resolveBlockCanvasItem(context_.scene->focusItem());
    }
    if (selectedBlock == nullptr) {
        if (context_.clearDetailsPane) {
            context_.clearDetailsPane();
        }
        return;
    }

    const int lineNumber = blockCanvasItemLineNumber(selectedBlock);
    const QString kind = blockCanvasItemKind(selectedBlock);
    const bool loaded = context_.loadDetailsForSelection
        ? context_.loadDetailsForSelection(kind, lineNumber)
        : false;
    if (!loaded && context_.setDetailsSelectionFallback) {
        context_.setDetailsSelectionFallback(lineNumber, kind);
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
    if (context_.scene == nullptr || item == nullptr) {
        return;
    }
    context_.scene->clearSelection();
    item->setSelected(true);
    context_.scene->setFocusItem(item);
    if (centerView && context_.view != nullptr) {
        context_.view->centerOn(item);
    }
}
}
