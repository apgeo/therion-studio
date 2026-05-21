#include "BlockEditorCanvasBoundaryController.h"

#include "BlockEditorCanvasItem.h"
#include "../TextEditorTab.h"

#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QTransform>
#include <QVariant>
#include <QLineF>

#include <limits>

namespace TherionStudio
{
BlockEditorCanvasBoundaryController::BlockEditorCanvasBoundaryController(TextEditorTab *owner)
    : owner_(owner)
{
}

int BlockEditorCanvasBoundaryController::resolveEndHintContainerStartLineAtScenePos(const QPointF &scenePos) const
{
    if (owner_ == nullptr || owner_->blockCanvasScene_ == nullptr) {
        return 0;
    }

    auto resolveFromItem = [](QGraphicsItem *item) -> int {
        while (item != nullptr) {
            const QVariant hintValue = item->data(kBlockEditorCanvasEndHintContainerLineDataRole);
            bool ok = false;
            const int lineNumber = hintValue.toInt(&ok);
            if (ok && lineNumber > 0) {
                return lineNumber;
            }
            item = item->parentItem();
        }
        return 0;
    };

    const int directLine = resolveFromItem(owner_->blockCanvasScene_->itemAt(scenePos, QTransform()));
    if (directLine > 0) {
        return directLine;
    }

    auto pointToSegmentDistance = [](const QPointF &point, const QLineF &segment) {
        const QPointF p1 = segment.p1();
        const QPointF p2 = segment.p2();
        const qreal dx = p2.x() - p1.x();
        const qreal dy = p2.y() - p1.y();
        const qreal lengthSquared = (dx * dx) + (dy * dy);
        if (lengthSquared <= std::numeric_limits<qreal>::epsilon()) {
            return QLineF(point, p1).length();
        }

        const qreal t = qBound<qreal>(0.0,
                                      ((point.x() - p1.x()) * dx + (point.y() - p1.y()) * dy) / lengthSquared,
                                      1.0);
        const QPointF projection(p1.x() + (t * dx), p1.y() + (t * dy));
        return QLineF(point, projection).length();
    };

    constexpr qreal kLineHitThreshold = 12.0;
    int bestLineNumber = 0;
    qreal bestDistance = std::numeric_limits<qreal>::max();

    for (QGraphicsLineItem *guideItem : owner_->blockContainerBoundaryGuideItems_) {
        if (guideItem == nullptr || !guideItem->isVisible()) {
            continue;
        }
        bool ok = false;
        const int lineNumber = guideItem->data(kBlockEditorCanvasEndHintContainerLineDataRole).toInt(&ok);
        if (!ok || lineNumber <= 0) {
            continue;
        }

        const QLineF sceneLine = QLineF(guideItem->mapToScene(guideItem->line().p1()),
                                        guideItem->mapToScene(guideItem->line().p2()));
        const qreal distance = pointToSegmentDistance(scenePos, sceneLine);
        if (distance <= kLineHitThreshold && distance < bestDistance) {
            bestDistance = distance;
            bestLineNumber = lineNumber;
        }
    }

    return bestLineNumber;
}
}
