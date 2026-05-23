#include "BlockEditorInsertionController.h"

#include "BlockEditorDocumentOutlineBuilder.h"
#include "BlockEditorDropTargetResolver.h"
#include "BlockEditorInsertionPlanner.h"
#include "BlockEditorInsertionTemplateBuilder.h"
#include "BlockEditorSourceController.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QtGlobal>

#include <utility>

namespace TherionStudio
{
BlockEditorInsertionController::BlockEditorInsertionController(BlockEditorInsertionContext context)
    : context_(std::move(context))
{
}

QString BlockEditorInsertionController::tr(const char *text) const
{
    return context_.translate != nullptr ? context_.translate(text) : QString::fromUtf8(text);
}

void BlockEditorInsertionController::insertFromDrop(const QString &kind, const QPointF &scenePos)
{
    if (context_.sourceContext == nullptr) {
        return;
    }

    if (context_.isBlocksModeSupportedForCurrentFile == nullptr || !context_.isBlocksModeSupportedForCurrentFile()) {
        QMessageBox::information(context_.dialogParent,
                                 tr("Blocks Mode"),
                                 tr("Blocks mode is available only for .th and .thconfig files."));
        return;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind.isEmpty()) {
        return;
    }

    const BlockEditorSourceController source(context_.sourceContext());
    if (!source.hasEditor()) {
        return;
    }

    const QString contents = source.text();
    const BlockEditorDocumentOutlineBuilder outlineBuilder(context_.documentOutlineContext);
    const BlockEditorDocumentOutline outline = outlineBuilder.buildFromContents(contents);
    const QStringList &existingLines = outline.lines;
    const QVector<BlockEditorDocumentEntry> &entries = outline.entries;
    const QHash<int, int> &entryIndexByStartLine = outline.entryIndexByStartLine;

    const BlockEditorDropTargetResolver targetResolver(context_.dropTargetContext);
    const auto sceneItemsByLine = targetResolver.collectSceneItemsByLine();
    const BlockEditorSceneVerticalPlacement verticalPlacement =
        targetResolver.resolveVerticalPlacement(entries, sceneItemsByLine, scenePos);

    int explicitEndHintContainerLine = 0;
    if (context_.resolveEndHintContainerStartLineAtScenePos != nullptr) {
        explicitEndHintContainerLine = context_.resolveEndHintContainerStartLineAtScenePos(scenePos);
    }

    QGraphicsItem *targetBlockItem = nullptr;
    if (!verticalPlacement.beforeAllBlocks
        && !verticalPlacement.afterAllBlocks
        && explicitEndHintContainerLine <= 0) {
        targetBlockItem = targetResolver.resolveTargetItem(entries, sceneItemsByLine, scenePos);
    }

    const int appendLineNumber = context_.appendLineNumber != nullptr ? context_.appendLineNumber() : 1;
    const BlockEditorInsertionPlanner insertionPlanner(context_.insertionPlannerContext);
    const BlockEditorInsertionPlan insertionPlan = insertionPlanner.planInsertion(normalizedKind,
                                                                                  entries,
                                                                                  entryIndexByStartLine,
                                                                                  verticalPlacement,
                                                                                  explicitEndHintContainerLine,
                                                                                  targetBlockItem,
                                                                                  scenePos,
                                                                                  appendLineNumber);
    if (!insertionPlan.compatible) {
        QMessageBox::warning(context_.dialogParent,
                             tr("Incompatible Drop"),
                             tr("`%1` cannot be inserted inside `%2`.")
                                 .arg(normalizedKind,
                                      insertionPlan.parentKind.isEmpty() ? tr("root") : insertionPlan.parentKind));
        return;
    }

    const BlockEditorInsertionTemplateBuilder templateBuilder;
    const QStringList linesToInsert = templateBuilder.buildLines(normalizedKind, existingLines, insertionPlan);

    QString errorMessage;
    if (!source.insertLinesBefore(insertionPlan.insertBeforeLine, linesToInsert, &errorMessage)) {
        QMessageBox::warning(context_.dialogParent, tr("Block Insert Failed"), errorMessage);
    }
}
}
