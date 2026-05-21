#include "BlockEditorMovePreviewController.h"

#include "../TextEditorTab.h"

#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QPen>

#include <limits>

namespace TherionStudio
{
BlockEditorMovePreviewController::BlockEditorMovePreviewController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorMovePreviewController::updateMovePreview(int sourceLineNumber, const QPointF &scenePos)
{
    if (owner_ == nullptr || owner_->blockCanvasScene_ == nullptr) {
        return;
    }

    const QList<QGraphicsItem *> sceneItems = owner_->blockCanvasScene_->items();

    auto subtreeBottomForItem = [](QGraphicsItem *rootItem) -> qreal {
        if (rootItem == nullptr) {
            return 0.0;
        }
        qreal subtreeBottom = rootItem->sceneBoundingRect().bottom();
        QList<QGraphicsItem *> stack;
        stack.append(rootItem);
        while (!stack.isEmpty()) {
            QGraphicsItem *current = stack.takeLast();
            if (current == nullptr) {
                continue;
            }
            subtreeBottom = qMax(subtreeBottom, current->sceneBoundingRect().bottom());
            for (QGraphicsItem *child : current->childItems()) {
                stack.append(child);
            }
        }
        return subtreeBottom;
    };

    qreal sceneTop = 0.0;
    qreal sceneBottom = 0.0;
    bool hasSceneVerticalBounds = false;
    for (QGraphicsItem *item : sceneItems) {
        QGraphicsItem *blockItem = owner_->resolveBlockCanvasItem(item);
        if (blockItem == nullptr) {
            continue;
        }
        const QRectF blockRect = blockItem->sceneBoundingRect();
        if (!hasSceneVerticalBounds) {
            sceneTop = blockRect.top();
            sceneBottom = blockRect.bottom();
            hasSceneVerticalBounds = true;
        } else {
            sceneTop = qMin(sceneTop, blockRect.top());
            sceneBottom = qMax(sceneBottom, blockRect.bottom());
        }
    }

    const int hintedContainerLine = owner_->resolveEndHintContainerStartLineAtScenePos(scenePos);
    if (hintedContainerLine > 0) {
        QGraphicsItem *hintedContainerItem = nullptr;
        for (QGraphicsItem *item : sceneItems) {
            QGraphicsItem *blockItem = owner_->resolveBlockCanvasItem(item);
            if (blockItem == nullptr || owner_->blockCanvasItemLineNumber(blockItem) != hintedContainerLine) {
                continue;
            }
            hintedContainerItem = blockItem;
            break;
        }
        if (hintedContainerItem != nullptr && owner_->blockCanvasItemLineNumber(hintedContainerItem) != sourceLineNumber) {
            qreal y = owner_->blockContainerBoundaryEndYByLine_.value(
                hintedContainerLine,
                subtreeBottomForItem(hintedContainerItem) + 10.0)
                + 4.0;
            const qreal left = 10.0;
            const qreal right = qMax<qreal>(left + 120.0, owner_->blockCanvasScene_->sceneRect().right() - 10.0);
            if (owner_->blockMovePreviewLine_ == nullptr) {
                QPen guidePen(QColor(QStringLiteral("#2f6fed")));
                guidePen.setWidthF(2.0);
                guidePen.setStyle(Qt::DashLine);
                owner_->blockMovePreviewLine_ = owner_->blockCanvasScene_->addLine(QLineF(left, y, right, y), guidePen);
                owner_->blockMovePreviewLine_->setZValue(10000.0);
            } else {
                owner_->blockMovePreviewLine_->setLine(QLineF(left, y, right, y));
                owner_->blockMovePreviewLine_->show();
            }
            return;
        }
    }

    const qreal outerDropThreshold = 16.0;
    const bool dropBeforeAllBlocks = hasSceneVerticalBounds && scenePos.y() < (sceneTop - outerDropThreshold);
    const bool dropAfterAllBlocks = hasSceneVerticalBounds && scenePos.y() > (sceneBottom + outerDropThreshold);
    if (dropBeforeAllBlocks || dropAfterAllBlocks) {
        const qreal y = dropBeforeAllBlocks ? (sceneTop - 4.0) : (sceneBottom + 4.0);
        const qreal left = 10.0;
        const qreal right = qMax<qreal>(left + 120.0, owner_->blockCanvasScene_->sceneRect().right() - 10.0);
        if (owner_->blockMovePreviewLine_ == nullptr) {
            QPen guidePen(QColor(QStringLiteral("#2f6fed")));
            guidePen.setWidthF(2.0);
            guidePen.setStyle(Qt::DashLine);
            owner_->blockMovePreviewLine_ = owner_->blockCanvasScene_->addLine(QLineF(left, y, right, y), guidePen);
            owner_->blockMovePreviewLine_->setZValue(10000.0);
        } else {
            owner_->blockMovePreviewLine_->setLine(QLineF(left, y, right, y));
            owner_->blockMovePreviewLine_->show();
        }
        return;
    }

    QGraphicsItem *targetBlockItem = owner_->resolveBlockCanvasItem(owner_->blockCanvasScene_->itemAt(scenePos, QTransform()));
    if (targetBlockItem != nullptr && owner_->blockCanvasItemLineNumber(targetBlockItem) == sourceLineNumber) {
        targetBlockItem = nullptr;
    }

    if (targetBlockItem == nullptr) {
        qreal bestDistance = std::numeric_limits<qreal>::max();
        for (QGraphicsItem *item : sceneItems) {
            QGraphicsItem *blockItem = owner_->resolveBlockCanvasItem(item);
            if (blockItem == nullptr || owner_->blockCanvasItemLineNumber(blockItem) == sourceLineNumber) {
                continue;
            }
            const QRectF blockRect = blockItem->sceneBoundingRect();
            const qreal topDistance = qAbs(blockRect.top() - scenePos.y());
            const qreal bottomDistance = qAbs(blockRect.bottom() - scenePos.y());
            const qreal distance = qMin(topDistance, bottomDistance);
            if (distance < bestDistance) {
                bestDistance = distance;
                targetBlockItem = blockItem;
            }
        }
    }

    if (targetBlockItem == nullptr) {
        clearMovePreview();
        return;
    }

    const QRectF targetRect = targetBlockItem->sceneBoundingRect();
    const bool insertAfterTarget = scenePos.y() > targetRect.center().y();
    const qreal y = insertAfterTarget ? (targetRect.bottom() + 4.0) : (targetRect.top() - 4.0);
    const qreal left = 10.0;
    const qreal right = qMax<qreal>(left + 120.0, owner_->blockCanvasScene_->sceneRect().right() - 10.0);

    if (owner_->blockMovePreviewLine_ == nullptr) {
        QPen guidePen(QColor(QStringLiteral("#2f6fed")));
        guidePen.setWidthF(2.0);
        guidePen.setStyle(Qt::DashLine);
        owner_->blockMovePreviewLine_ = owner_->blockCanvasScene_->addLine(QLineF(left, y, right, y), guidePen);
        owner_->blockMovePreviewLine_->setZValue(10000.0);
    } else {
        owner_->blockMovePreviewLine_->setLine(QLineF(left, y, right, y));
        owner_->blockMovePreviewLine_->show();
    }
}

void BlockEditorMovePreviewController::clearMovePreview()
{
    if (owner_ != nullptr && owner_->blockMovePreviewLine_ != nullptr) {
        owner_->blockMovePreviewLine_->hide();
    }
}
}
