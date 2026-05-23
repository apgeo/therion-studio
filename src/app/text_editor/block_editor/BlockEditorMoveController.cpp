#include "BlockEditorMoveController.h"

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"
#include "BlockEditorMovePlanner.h"
#include "BlockEditorMoveSourceRewriter.h"
#include "BlockEditorSourceController.h"
#include "../TextEditorTab.h"

#include <QGraphicsScene>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScopeGuard>

namespace TherionStudio
{
BlockEditorMoveController::BlockEditorMoveController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorMoveController::moveBlock(int lineNumber, const QPointF &scenePos)
{
    if (owner_ == nullptr) {
        return;
    }

    const auto clearPreviewOnExit = qScopeGuard([this]() {
        owner_->clearBlockMovePreview();
    });

    if (!owner_->isBlocksModeSupportedForCurrentFile()) {
        return;
    }
    if (lineNumber <= 0 || owner_->editor_ == nullptr || owner_->blockCanvasScene_ == nullptr) {
        return;
    }

    const QString contents = owner_->editor_->toPlainText();
    const BlockEditorDocumentOutlineBuilder outlineBuilder(owner_);
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

    const BlockEditorDropTargetResolver targetResolver(owner_);
    const auto sceneItemsByLine = targetResolver.collectSceneItemsByLine();
    const BlockEditorSceneVerticalPlacement verticalPlacement =
        targetResolver.resolveVerticalPlacement(entries, sceneItemsByLine, scenePos);
    const int explicitEndHintContainerLine = owner_->resolveEndHintContainerStartLineAtScenePos(scenePos);
    const BlockEditorMovePlanner movePlanner(owner_);
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
        QMessageBox::warning(owner_, TextEditorTab::tr("Reorder Block"), TextEditorTab::tr("Cannot move a block inside itself."));
        return;
    }
    if (movePlan.insertBeforeLineOriginal > sourceEntry.startLine
        && movePlan.insertBeforeLineOriginal <= sourceEntry.endLine + 1) {
        return;
    }

    if (!owner_->isCompatibleChildKindForBlocks(movePlan.destinationParentKind, sourceEntry.kind)) {
        QMessageBox::warning(owner_,
                             TextEditorTab::tr("Reorder Block"),
                             TextEditorTab::tr("`%1` cannot be moved under `%2`.")
                                 .arg(sourceEntry.kind,
                                      movePlan.destinationParentKind.isEmpty() ? TextEditorTab::tr("root") : movePlan.destinationParentKind));
        return;
    }

    const BlockEditorMoveSourceRewriter sourceRewriter;
    const BlockEditorMoveRewriteResult rewriteResult =
        sourceRewriter.rewriteMovedBlock(lines, sourceEntry, movePlan.insertBeforeLineOriginal);
    if (!rewriteResult.applied) {
        return;
    }

    BlockEditorSourceController(owner_->blockEditorSourceContext()).replaceWithLines(contents, rewriteResult.lines);
}
}
