#include "BlockEditorApplyExecutor.h"

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
    if (context_.translate) {
        return context_.translate(text);
    }
    return QString::fromUtf8(text);
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
    if (!source.replaceLine(*context_.selectedLineNumber, updatedLine)) {
        return;
    }

    context_.selectBlockInCanvasAndDetails(*context_.selectedLineNumber);
    context_.refreshApplyState();
}
}
