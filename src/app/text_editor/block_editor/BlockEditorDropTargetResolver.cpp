#include "BlockEditorDropTargetResolver.h"

#include "BlockEditorDocumentOutlineBuilder.h"
#include "../TextEditorTab.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QRectF>
#include <QTransform>

#include <limits>

namespace TherionStudio
{
BlockEditorDropTargetResolver::BlockEditorDropTargetResolver(const TextEditorTab *owner)
    : owner_(owner)
{
}

BlockEditorDropTargetResolver::SceneItemsByLine BlockEditorDropTargetResolver::collectSceneItemsByLine() const
{
    SceneItemsByLine sceneItemsByLine;
    if (owner_ == nullptr || owner_->blockCanvasScene_ == nullptr) {
        return sceneItemsByLine;
    }

    const QList<QGraphicsItem *> sceneItems = owner_->blockCanvasScene_->items();
    for (QGraphicsItem *item : sceneItems) {
        if (item == nullptr) {
            continue;
        }
        const int lineNumber = owner_->blockCanvasItemLineNumber(item);
        if (lineNumber <= 0) {
            continue;
        }
        sceneItemsByLine.insert(lineNumber, item);
    }

    return sceneItemsByLine;
}

BlockEditorSceneVerticalPlacement BlockEditorDropTargetResolver::resolveVerticalPlacement(
    const QVector<BlockEditorDocumentEntry> &entries,
    const SceneItemsByLine &sceneItemsByLine,
    const QPointF &scenePos) const
{
    qreal sceneTop = 0.0;
    qreal sceneBottom = 0.0;
    bool hasSceneVerticalBounds = false;
    for (const BlockEditorDocumentEntry &entry : entries) {
        const auto sceneItemIt = sceneItemsByLine.constFind(entry.startLine);
        if (sceneItemIt == sceneItemsByLine.cend() || sceneItemIt.value() == nullptr) {
            continue;
        }
        const QRectF blockRect = sceneItemIt.value()->sceneBoundingRect();
        if (!hasSceneVerticalBounds) {
            sceneTop = blockRect.top();
            sceneBottom = blockRect.bottom();
            hasSceneVerticalBounds = true;
        } else {
            sceneTop = qMin(sceneTop, blockRect.top());
            sceneBottom = qMax(sceneBottom, blockRect.bottom());
        }
    }

    constexpr qreal outerDropThreshold = 16.0;
    return BlockEditorSceneVerticalPlacement{
        hasSceneVerticalBounds && scenePos.y() < (sceneTop - outerDropThreshold),
        hasSceneVerticalBounds && scenePos.y() > (sceneBottom + outerDropThreshold),
    };
}

QGraphicsItem *BlockEditorDropTargetResolver::resolveTargetItem(
    const QVector<BlockEditorDocumentEntry> &entries,
    const SceneItemsByLine &sceneItemsByLine,
    const QPointF &scenePos,
    int directExcludedLineNumber,
    const EntryPredicate &excludeEntry) const
{
    if (owner_ == nullptr || owner_->blockCanvasScene_ == nullptr) {
        return nullptr;
    }

    QGraphicsItem *targetBlockItem = owner_->resolveBlockCanvasItem(owner_->blockCanvasScene_->itemAt(scenePos, QTransform()));
    if (targetBlockItem != nullptr) {
        const int targetLine = owner_->blockCanvasItemLineNumber(targetBlockItem);
        if (targetLine == directExcludedLineNumber) {
            targetBlockItem = nullptr;
        }
    }

    if (targetBlockItem != nullptr) {
        return targetBlockItem;
    }

    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (const BlockEditorDocumentEntry &entry : entries) {
        if (excludeEntry && excludeEntry(entry)) {
            continue;
        }
        const auto sceneItemIt = sceneItemsByLine.constFind(entry.startLine);
        if (sceneItemIt == sceneItemsByLine.cend() || sceneItemIt.value() == nullptr) {
            continue;
        }
        const QRectF blockRect = sceneItemIt.value()->sceneBoundingRect();
        const qreal topDistance = qAbs(blockRect.top() - scenePos.y());
        const qreal bottomDistance = qAbs(blockRect.bottom() - scenePos.y());
        const qreal distance = qMin(topDistance, bottomDistance);
        if (distance < bestDistance) {
            bestDistance = distance;
            targetBlockItem = sceneItemIt.value();
        }
    }

    return targetBlockItem;
}
}
