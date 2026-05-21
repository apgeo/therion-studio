#include "BlockEditorInsertionPlanner.h"

#include "BlockEditorDirectiveRules.h"
#include "../TextEditorTab.h"

#include <QGraphicsItem>
#include <QRectF>

namespace TherionStudio
{
using namespace BlockEditorDirectiveRules;

BlockEditorInsertionPlanner::BlockEditorInsertionPlanner(const TextEditorTab *owner)
    : owner_(owner)
{
}

BlockEditorInsertionPlan BlockEditorInsertionPlanner::planInsertion(
    const QString &normalizedKind,
    const QVector<BlockEditorDocumentEntry> &entries,
    const QHash<int, int> &entryIndexByStartLine,
    const BlockEditorSceneVerticalPlacement &verticalPlacement,
    int explicitEndHintContainerLine,
    QGraphicsItem *targetBlockItem,
    const QPointF &scenePos,
    int appendLineNumber) const
{
    BlockEditorInsertionPlan plan;
    plan.insertBeforeLine = appendLineNumber;
    if (owner_ == nullptr) {
        plan.compatible = false;
        return plan;
    }

    if (verticalPlacement.beforeAllBlocks) {
        plan.insertBeforeLine = 1;
        plan.parentLine = 0;
    } else if (explicitEndHintContainerLine > 0) {
        const auto hintedEntryIt = entryIndexByStartLine.constFind(explicitEndHintContainerLine);
        if (hintedEntryIt != entryIndexByStartLine.cend()) {
            const BlockEditorDocumentEntry hintedEntry = entries.at(*hintedEntryIt);
            plan.insertBeforeLine = hintedEntry.endLine + 1;
            plan.parentLine = hintedEntry.parentLine;
        }
    } else if (targetBlockItem != nullptr) {
        const auto targetIndexIt = entryIndexByStartLine.constFind(owner_->blockCanvasItemLineNumber(targetBlockItem));
        if (targetIndexIt != entryIndexByStartLine.cend()) {
            const BlockEditorDocumentEntry targetEntry = entries.at(*targetIndexIt);
            plan.parentLine = targetEntry.parentLine;
            plan.insertBeforeLine = targetEntry.startLine;

            const QRectF targetRect = targetBlockItem->sceneBoundingRect();
            const qreal edgeDropThreshold = qMin<qreal>(10.0, targetRect.height() * 0.25);
            const bool nearTopEdge = scenePos.y() <= (targetRect.top() + edgeDropThreshold);
            const bool nearBottomEdge = scenePos.y() >= (targetRect.bottom() - edgeDropThreshold);
            const bool preferBetweenInsertion = nearTopEdge || nearBottomEdge;

            const bool targetAcceptsSourceAsChild = isContainerBlockDirective(targetEntry.kind)
                && owner_->isCompatibleChildKindForBlocks(targetEntry.kind, normalizedKind);
            if (targetAcceptsSourceAsChild) {
                if (!preferBetweenInsertion) {
                    plan.parentLine = targetEntry.startLine;
                    plan.insertBeforeLine = targetEntry.endLine;
                } else if (nearBottomEdge) {
                    int firstChildStartLine = 0;
                    for (const BlockEditorDocumentEntry &entry : entries) {
                        if (entry.parentLine != targetEntry.startLine) {
                            continue;
                        }
                        if (firstChildStartLine == 0 || entry.startLine < firstChildStartLine) {
                            firstChildStartLine = entry.startLine;
                        }
                    }
                    plan.parentLine = targetEntry.startLine;
                    plan.insertBeforeLine = firstChildStartLine > 0 ? firstChildStartLine : targetEntry.endLine;
                } else {
                    const qreal targetCenterY = targetRect.center().y();
                    const bool insertAfterTarget = scenePos.y() > targetCenterY;
                    plan.insertBeforeLine = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
                    plan.parentLine = targetEntry.parentLine;
                }
            } else {
                const qreal targetCenterY = targetRect.center().y();
                const bool insertAfterTarget = scenePos.y() > targetCenterY;
                plan.insertBeforeLine = insertAfterTarget ? (targetEntry.endLine + 1) : targetEntry.startLine;
                plan.parentLine = targetEntry.parentLine;
            }
        }
    }

    if (plan.parentLine > 0) {
        const auto parentEntryIt = entryIndexByStartLine.constFind(plan.parentLine);
        if (parentEntryIt != entryIndexByStartLine.cend()) {
            plan.parentKind = entries.at(*parentEntryIt).kind;
        }
    }

    if (owner_->isCompatibleChildKindForBlocks(plan.parentKind, normalizedKind)) {
        return plan;
    }

    int incompatibleParentLine = plan.parentLine;
    while (incompatibleParentLine > 0) {
        const auto incompatibleParentIt = entryIndexByStartLine.constFind(incompatibleParentLine);
        if (incompatibleParentIt == entryIndexByStartLine.cend()) {
            break;
        }
        const BlockEditorDocumentEntry incompatibleParentEntry = entries.at(*incompatibleParentIt);
        const int candidateParentLine = incompatibleParentEntry.parentLine;
        QString candidateParentKind;
        if (candidateParentLine > 0) {
            const auto candidateParentIt = entryIndexByStartLine.constFind(candidateParentLine);
            if (candidateParentIt != entryIndexByStartLine.cend()) {
                candidateParentKind = entries.at(*candidateParentIt).kind;
            }
        }
        if (owner_->isCompatibleChildKindForBlocks(candidateParentKind, normalizedKind)) {
            plan.insertBeforeLine = incompatibleParentEntry.endLine + 1;
            plan.parentLine = candidateParentLine;
            plan.parentKind = candidateParentKind;
            return plan;
        }
        incompatibleParentLine = candidateParentLine;
    }

    if (owner_->isCompatibleChildKindForBlocks(QString(), normalizedKind)) {
        plan.insertBeforeLine = appendLineNumber;
        plan.parentLine = 0;
        plan.parentKind.clear();
        return plan;
    }

    plan.compatible = false;
    return plan;
}
}
