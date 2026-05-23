#include "BlockEditorMovePlanner.h"

#include "BlockEditorDirectiveRules.h"

#include <QGraphicsItem>
#include <QRectF>

#include <utility>

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

BlockEditorMovePlanner::BlockEditorMovePlanner(BlockEditorMovePlannerContext context)
    : context_(std::move(context))
{
}

BlockEditorMovePlan BlockEditorMovePlanner::planMove(
    const BlockEditorDocumentEntry &sourceEntry,
    const QVector<BlockEditorDocumentEntry> &entries,
    const QHash<int, int> &entryIndexByStartLine,
    const BlockEditorDropTargetResolver::SceneItemsByLine &sceneItemsByLine,
    const BlockEditorSceneVerticalPlacement &verticalPlacement,
    int explicitEndHintContainerLine,
    const QPointF &scenePos,
    int appendLineNumber) const
{
    BlockEditorMovePlan plan;
    if (!context_.blockCanvasItemLineNumber || !context_.isCompatibleChildKindForBlocks) {
        return plan;
    }

    if (verticalPlacement.beforeAllBlocks) {
        plan.destinationParentLine = 0;
        plan.insertBeforeLineOriginal = 1;
        plan.resolved = true;
    } else if (verticalPlacement.afterAllBlocks) {
        plan.destinationParentLine = 0;
        plan.insertBeforeLineOriginal = appendLineNumber;
        plan.resolved = true;
    } else if (explicitEndHintContainerLine > 0) {
        const auto hintedIndexIt = entryIndexByStartLine.constFind(explicitEndHintContainerLine);
        if (hintedIndexIt != entryIndexByStartLine.cend()) {
            const BlockEditorDocumentEntry hintedEntry = entries.at(*hintedIndexIt);
            plan.destinationParentLine = hintedEntry.parentLine;
            plan.insertBeforeLineOriginal = hintedEntry.endLine + 1;
            plan.resolved = true;
        }
    }

    auto isEntryInsideSourceSubtree = [&sourceEntry](const BlockEditorDocumentEntry &entry) {
        return entry.startLine >= sourceEntry.startLine && entry.startLine <= sourceEntry.endLine;
    };

    if (!plan.resolved) {
        const BlockEditorDropTargetResolver targetResolver(context_.dropTargetContext);
        QGraphicsItem *targetBlockItem = targetResolver.resolveTargetItem(entries,
                                                                          sceneItemsByLine,
                                                                          scenePos,
                                                                          sourceEntry.startLine,
                                                                          isEntryInsideSourceSubtree);
        if (targetBlockItem == nullptr) {
            return plan;
        }

        const auto targetIndexIt = entryIndexByStartLine.constFind(context_.blockCanvasItemLineNumber(targetBlockItem));
        if (targetIndexIt == entryIndexByStartLine.cend()) {
            return plan;
        }
        const BlockEditorDocumentEntry targetEntry = entries.at(*targetIndexIt);

        plan.destinationParentLine = targetEntry.parentLine;
        plan.insertBeforeLineOriginal = targetEntry.startLine;
        plan.resolved = true;

        const QRectF targetRect = targetBlockItem->sceneBoundingRect();
        const qreal edgeDropThreshold = qMin<qreal>(10.0, targetRect.height() * 0.25);
        const bool nearTopEdge = scenePos.y() <= (targetRect.top() + edgeDropThreshold);
        const bool nearBottomEdge = scenePos.y() >= (targetRect.bottom() - edgeDropThreshold);
        const bool preferBetweenInsertion = nearTopEdge || nearBottomEdge;

        const bool targetAcceptsSourceAsChild = isContainerBlockDirective(targetEntry.kind)
            && context_.isCompatibleChildKindForBlocks(targetEntry.kind, sourceEntry.kind);
        if (targetAcceptsSourceAsChild && targetEntry.startLine != sourceEntry.startLine) {
            if (!preferBetweenInsertion) {
                plan.destinationParentLine = targetEntry.startLine;
                plan.insertBeforeLineOriginal = targetEntry.endLine;
            } else if (nearBottomEdge) {
                int firstChildStartLine = 0;
                for (const BlockEditorDocumentEntry &entry : entries) {
                    if (entry.parentLine != targetEntry.startLine || isEntryInsideSourceSubtree(entry)) {
                        continue;
                    }
                    if (firstChildStartLine == 0 || entry.startLine < firstChildStartLine) {
                        firstChildStartLine = entry.startLine;
                    }
                }
                plan.destinationParentLine = targetEntry.startLine;
                plan.insertBeforeLineOriginal = firstChildStartLine > 0 ? firstChildStartLine : targetEntry.endLine;
            } else {
                const qreal targetCenterY = targetRect.center().y();
                const bool insertAfterTarget = scenePos.y() > targetCenterY;
                plan.insertBeforeLineOriginal = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
                plan.destinationParentLine = targetEntry.parentLine;
            }
        } else {
            const qreal targetCenterY = targetRect.center().y();
            const bool insertAfterTarget = scenePos.y() > targetCenterY;
            plan.insertBeforeLineOriginal = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
            plan.destinationParentLine = targetEntry.parentLine;
        }
    }

    if (plan.destinationParentLine > 0) {
        const auto parentEntryIt = entryIndexByStartLine.constFind(plan.destinationParentLine);
        if (parentEntryIt != entryIndexByStartLine.cend()) {
            plan.destinationParentKind = entries.at(*parentEntryIt).kind;
        }
    }

    return plan;
}
}
