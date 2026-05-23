#include "BlockEditorMovePreviewController.h"

#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QPen>

#include <limits>
#include <utility>

namespace TherionStudio
{
BlockEditorMovePreviewController::BlockEditorMovePreviewController(BlockEditorMovePreviewContext context)
    : context_(std::move(context))
{
}

bool BlockEditorMovePreviewController::hasRequiredContext() const
{
    return context_.scene != nullptr
        && context_.previewLine != nullptr
        && context_.containerBoundaryEndYByLine != nullptr
        && context_.resolveBlockCanvasItem
        && context_.blockCanvasItemLineNumber
        && context_.resolveEndHintContainerStartLineAtScenePos;
}

void BlockEditorMovePreviewController::showPreviewLine(qreal y)
{
    const qreal left = 10.0;
    const qreal right = qMax<qreal>(left + 120.0, context_.scene->sceneRect().right() - 10.0);
    if (*context_.previewLine == nullptr) {
        QPen guidePen(QColor(QStringLiteral("#2f6fed")));
        guidePen.setWidthF(2.0);
        guidePen.setStyle(Qt::DashLine);
        *context_.previewLine = context_.scene->addLine(QLineF(left, y, right, y), guidePen);
        (*context_.previewLine)->setZValue(10000.0);
    } else {
        (*context_.previewLine)->setLine(QLineF(left, y, right, y));
        (*context_.previewLine)->show();
    }
}

void BlockEditorMovePreviewController::updateMovePreview(int sourceLineNumber, const QPointF &scenePos)
{
    if (!hasRequiredContext()) {
        return;
    }

    const QList<QGraphicsItem *> sceneItems = context_.scene->items();

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
        QGraphicsItem *blockItem = context_.resolveBlockCanvasItem(item);
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

    const int hintedContainerLine = context_.resolveEndHintContainerStartLineAtScenePos(scenePos);
    if (hintedContainerLine > 0) {
        QGraphicsItem *hintedContainerItem = nullptr;
        for (QGraphicsItem *item : sceneItems) {
            QGraphicsItem *blockItem = context_.resolveBlockCanvasItem(item);
            if (blockItem == nullptr || context_.blockCanvasItemLineNumber(blockItem) != hintedContainerLine) {
                continue;
            }
            hintedContainerItem = blockItem;
            break;
        }
        if (hintedContainerItem != nullptr && context_.blockCanvasItemLineNumber(hintedContainerItem) != sourceLineNumber) {
            qreal y = context_.containerBoundaryEndYByLine->value(
                hintedContainerLine,
                subtreeBottomForItem(hintedContainerItem) + 10.0)
                + 4.0;
            showPreviewLine(y);
            return;
        }
    }

    const qreal outerDropThreshold = 16.0;
    const bool dropBeforeAllBlocks = hasSceneVerticalBounds && scenePos.y() < (sceneTop - outerDropThreshold);
    const bool dropAfterAllBlocks = hasSceneVerticalBounds && scenePos.y() > (sceneBottom + outerDropThreshold);
    if (dropBeforeAllBlocks || dropAfterAllBlocks) {
        const qreal y = dropBeforeAllBlocks ? (sceneTop - 4.0) : (sceneBottom + 4.0);
        showPreviewLine(y);
        return;
    }

    QGraphicsItem *targetBlockItem = context_.resolveBlockCanvasItem(context_.scene->itemAt(scenePos, QTransform()));
    if (targetBlockItem != nullptr && context_.blockCanvasItemLineNumber(targetBlockItem) == sourceLineNumber) {
        targetBlockItem = nullptr;
    }

    if (targetBlockItem == nullptr) {
        qreal bestDistance = std::numeric_limits<qreal>::max();
        for (QGraphicsItem *item : sceneItems) {
            QGraphicsItem *blockItem = context_.resolveBlockCanvasItem(item);
            if (blockItem == nullptr || context_.blockCanvasItemLineNumber(blockItem) == sourceLineNumber) {
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
    showPreviewLine(y);
}

void BlockEditorMovePreviewController::clearMovePreview()
{
    if (context_.previewLine != nullptr && *context_.previewLine != nullptr) {
        (*context_.previewLine)->hide();
    }
}
}
