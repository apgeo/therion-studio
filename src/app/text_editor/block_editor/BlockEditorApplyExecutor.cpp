#include "BlockEditorApplyExecutor.h"

#include "BlockEditorSourceText.h"
#include "../../../core/TherionDocumentEditor.h"

#include <QLabel>

#include <utility>

namespace TherionStudio
{
BlockEditorApplyExecutor::BlockEditorApplyExecutor(BlockEditorApplyExecutorContext context)
    : context_(std::move(context))
{
}

void BlockEditorApplyExecutor::applyChanges()
{
    if (context_.tearingDown == nullptr
        || context_.detailsPopulating == nullptr
        || context_.selectedLineNumber == nullptr
        || !context_.sourceContext
        || !context_.buildUpdatedLine
        || !context_.selectBlockInCanvasAndDetails
        || !context_.refreshApplyState
        || (*context_.tearingDown)
        || (*context_.detailsPopulating)) {
        return;
    }

    QString updatedLine;
    QString validationError;
    if (!context_.buildUpdatedLine(&updatedLine, &validationError)) {
        if (!validationError.isEmpty() && context_.statusLabel != nullptr) {
            context_.statusLabel->setStyleSheet(QStringLiteral("color: #c0392b;"));
            context_.statusLabel->setText(context_.baseStatusText != nullptr
                                              ? QStringLiteral("%1 — %2").arg(*context_.baseStatusText, validationError)
                                              : validationError);
        }
        return;
    }

    const BlockEditorSourceController source(context_.sourceContext());
    QStringList lines = source.normalizedLines();
    BlockEditorLogicalLine logicalLine;
    if (!blockEditorResolveLogicalLineAtLine(lines, *context_.selectedLineNumber, &logicalLine)) {
        return;
    }

    if (updatedLine == logicalLine.text) {
        context_.refreshApplyState();
        return;
    }

    TherionSourceTextEdit sourceEdit;
    if (!blockEditorSourceLineRangeReplacementEdit(source.text(),
                                                   logicalLine.startLine,
                                                   logicalLine.endLine,
                                                   QStringList{updatedLine},
                                                   &sourceEdit)) {
        return;
    }

    if (!source.applyTextEdit(sourceEdit)) {
        return;
    }

    context_.selectBlockInCanvasAndDetails(*context_.selectedLineNumber);
    context_.refreshApplyState();
}
}
