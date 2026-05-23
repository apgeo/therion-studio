#include "BlockEditorMoveController.h"

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"
#include "BlockEditorMovePlanner.h"
#include "BlockEditorMoveSourceRewriter.h"
#include "BlockEditorSourceController.h"

#include <QGraphicsScene>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScopeGuard>

#include <utility>

namespace TherionStudio
{
BlockEditorMoveController::BlockEditorMoveController(BlockEditorMoveContext context)
    : context_(std::move(context))
{
}

QString BlockEditorMoveController::tr(const char *text) const
{
    return context_.translate != nullptr ? context_.translate(text) : QString::fromUtf8(text);
}

void BlockEditorMoveController::moveBlock(int lineNumber, const QPointF &scenePos)
{
    if (context_.sourceContext == nullptr) {
        return;
    }

    const auto clearPreviewOnExit = qScopeGuard([this]() {
        if (context_.clearBlockMovePreview != nullptr) {
            context_.clearBlockMovePreview();
        }
    });

    if (context_.isBlocksModeSupportedForCurrentFile == nullptr || !context_.isBlocksModeSupportedForCurrentFile()) {
        return;
    }
    const BlockEditorSourceController source(context_.sourceContext());
    if (lineNumber <= 0 || !source.hasEditor() || context_.dropTargetContext.scene == nullptr) {
        return;
    }

    const QString contents = source.text();
    const BlockEditorDocumentOutlineBuilder outlineBuilder(context_.documentOutlineContext);
    const BlockEditorDocumentOutline outline = outlineBuilder.buildFromContents(contents);
    QStringList lines = outline.lines;
    const QVector<BlockEditorDocumentEntry> &entries = outline.entries;
    const QHash<int, int> &entryIndexByStartLine = outline.entryIndexByStartLine;

    const auto sourceIndexIt = entryIndexByStartLine.constFind(lineNumber);
    if (sourceIndexIt == entryIndexByStartLine.cend()) {
        return;
    }
    const BlockEditorDocumentEntry sourceEntry = entries.at(*sourceIndexIt);
    if (sourceEntry.kind == QStringLiteral("encoding")) {
        return;
    }

    const BlockEditorDropTargetResolver targetResolver(context_.dropTargetContext);
    const auto sceneItemsByLine = targetResolver.collectSceneItemsByLine();
    const BlockEditorSceneVerticalPlacement verticalPlacement =
        targetResolver.resolveVerticalPlacement(entries, sceneItemsByLine, scenePos);
    const int explicitEndHintContainerLine = context_.resolveEndHintContainerStartLineAtScenePos != nullptr
        ? context_.resolveEndHintContainerStartLineAtScenePos(scenePos)
        : 0;
    const BlockEditorMovePlanner movePlanner(context_.movePlannerContext);
    const BlockEditorMovePlan movePlan = movePlanner.planMove(sourceEntry,
                                                              entries,
                                                              entryIndexByStartLine,
                                                              sceneItemsByLine,
                                                              verticalPlacement,
                                                              explicitEndHintContainerLine,
                                                              scenePos,
                                                              lines.size() + 1);
    if (!movePlan.resolved) {
        return;
    }

    if (movePlan.destinationParentLine >= sourceEntry.startLine && movePlan.destinationParentLine <= sourceEntry.endLine) {
        QMessageBox::warning(context_.dialogParent, tr("Reorder Block"), tr("Cannot move a block inside itself."));
        return;
    }
    if (movePlan.insertBeforeLineOriginal > sourceEntry.startLine
        && movePlan.insertBeforeLineOriginal <= sourceEntry.endLine + 1) {
        return;
    }

    if (context_.isCompatibleChildKindForBlocks == nullptr
        || !context_.isCompatibleChildKindForBlocks(movePlan.destinationParentKind, sourceEntry.kind)) {
        QMessageBox::warning(context_.dialogParent,
                             tr("Reorder Block"),
                             tr("`%1` cannot be moved under `%2`.")
                                 .arg(sourceEntry.kind,
                                      movePlan.destinationParentKind.isEmpty() ? tr("root") : movePlan.destinationParentKind));
        return;
    }

    const BlockEditorMoveSourceRewriter sourceRewriter;
    const BlockEditorMoveRewriteResult rewriteResult =
        sourceRewriter.rewriteMovedBlock(lines, sourceEntry, movePlan.insertBeforeLineOriginal);
    if (!rewriteResult.applied) {
        return;
    }

    source.replaceWithLines(contents, rewriteResult.lines);
}
}
