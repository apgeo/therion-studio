#include "BlockEditorInsertionController.h"

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"
#include "BlockEditorInsertionPlanner.h"
#include "BlockEditorInsertionTemplateBuilder.h"
#include "BlockEditorSourceController.h"
#include "../TextEditorTab.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QtGlobal>

namespace TherionStudio
{
BlockEditorInsertionController::BlockEditorInsertionController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorInsertionController::insertFromDrop(const QString &kind, const QPointF &scenePos)
{
    if (owner_ == nullptr) {
        return;
    }

    if (!owner_->isBlocksModeSupportedForCurrentFile()) {
        QMessageBox::information(owner_,
                                 TextEditorTab::tr("Blocks Mode"),
                                 TextEditorTab::tr("Blocks mode is available only for .th and .thconfig files."));
        return;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind.isEmpty()) {
        return;
    }

    const QString contents = owner_->editor_->toPlainText();
    const BlockEditorDocumentOutlineBuilder outlineBuilder(owner_);
    const BlockEditorDocumentOutline outline = outlineBuilder.buildFromContents(contents);
    const QStringList &existingLines = outline.lines;
    const QVector<BlockEditorDocumentEntry> &entries = outline.entries;
    const QHash<int, int> &entryIndexByStartLine = outline.entryIndexByStartLine;

    const BlockEditorDropTargetResolver targetResolver(owner_);
    const auto sceneItemsByLine = targetResolver.collectSceneItemsByLine();
    const BlockEditorSceneVerticalPlacement verticalPlacement =
        targetResolver.resolveVerticalPlacement(entries, sceneItemsByLine, scenePos);

    int explicitEndHintContainerLine = 0;
    if (owner_->blockCanvasScene_ != nullptr) {
        explicitEndHintContainerLine = owner_->resolveEndHintContainerStartLineAtScenePos(scenePos);
    }

    QGraphicsItem *targetBlockItem = nullptr;
    if (!verticalPlacement.beforeAllBlocks
        && !verticalPlacement.afterAllBlocks
        && explicitEndHintContainerLine <= 0) {
        targetBlockItem = targetResolver.resolveTargetItem(entries, sceneItemsByLine, scenePos);
    }

    const int appendLineNumber = qMax(1, owner_->editor_->document()->blockCount() + 1);
    const BlockEditorInsertionPlanner insertionPlanner(owner_);
    const BlockEditorInsertionPlan insertionPlan = insertionPlanner.planInsertion(normalizedKind,
                                                                                  entries,
                                                                                  entryIndexByStartLine,
                                                                                  verticalPlacement,
                                                                                  explicitEndHintContainerLine,
                                                                                  targetBlockItem,
                                                                                  scenePos,
                                                                                  appendLineNumber);
    if (!insertionPlan.compatible) {
        QMessageBox::warning(owner_,
                             TextEditorTab::tr("Incompatible Drop"),
                             TextEditorTab::tr("`%1` cannot be inserted inside `%2`.")
                                 .arg(normalizedKind,
                                      insertionPlan.parentKind.isEmpty() ? TextEditorTab::tr("root") : insertionPlan.parentKind));
        return;
    }

    const BlockEditorInsertionTemplateBuilder templateBuilder;
    const QStringList linesToInsert = templateBuilder.buildLines(normalizedKind, existingLines, insertionPlan);

    QString errorMessage;
    const BlockEditorSourceController source(owner_->blockEditorSourceContext());
    if (!source.insertLinesBefore(insertionPlan.insertBeforeLine, linesToInsert, &errorMessage)) {
        QMessageBox::warning(owner_, TextEditorTab::tr("Block Insert Failed"), errorMessage);
    }
}
}
