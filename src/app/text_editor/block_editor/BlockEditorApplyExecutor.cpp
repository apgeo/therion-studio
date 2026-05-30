#include "BlockEditorApplyExecutor.h"

#include <QCoreApplication>

#include "BlockEditorSourceText.h"

#include <QMessageBox>

#include <utility>

namespace TherionStudio
{
BlockEditorApplyExecutor::BlockEditorApplyExecutor(BlockEditorApplyExecutorContext context)
    : context_(std::move(context))
{
}

QString BlockEditorApplyExecutor::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorApplyExecutor", text);
}

void BlockEditorApplyExecutor::applyChanges()
{
    if (context_.tearingDown == nullptr
        || context_.selectedLineNumber == nullptr
        || !context_.sourceContext
        || !context_.buildUpdatedLine
        || !context_.selectBlockInCanvasAndDetails
        || !context_.refreshApplyState
        || (*context_.tearingDown)) {
        return;
    }

    QString updatedLine;
    QString validationError;
    if (!context_.buildUpdatedLine(&updatedLine, &validationError)) {
        if (!validationError.isEmpty()) {
            QMessageBox::warning(context_.dialogParent, tr("Configure Block"), validationError);
        }
        return;
    }

    const BlockEditorSourceController source(context_.sourceContext());
    QStringList lines = source.normalizedLines();
    BlockEditorLogicalLine logicalLine;
    if (!blockEditorResolveLogicalLineAtLine(lines, *context_.selectedLineNumber, &logicalLine)) {
        return;
    }

    if (!blockEditorReplaceSourceLineRange(&lines,
                                           logicalLine.startLine,
                                           logicalLine.endLine,
                                           QStringList{updatedLine})) {
        return;
    }

    source.replaceText(blockEditorJoinSourceLines(source.text(), lines));

    context_.selectBlockInCanvasAndDetails(*context_.selectedLineNumber);
    context_.refreshApplyState();
}
}
