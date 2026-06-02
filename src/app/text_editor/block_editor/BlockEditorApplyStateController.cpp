#include "BlockEditorApplyStateController.h"

#include <QLabel>

#include <utility>

namespace TherionStudio
{
BlockEditorApplyStateController::BlockEditorApplyStateController(BlockEditorApplyStateContext context)
    : context_(std::move(context))
{
}

QLabel *BlockEditorApplyStateController::statusLabel() const
{
    return context_.statusLabel != nullptr ? *context_.statusLabel : nullptr;
}

void BlockEditorApplyStateController::refreshApplyState()
{
    if (context_.tearingDown == nullptr
        || context_.detailsPopulating == nullptr
        || context_.selectedLineNumber == nullptr
        || context_.baseStatusText == nullptr
        || !context_.sourceContext
        || !context_.buildUpdatedLine
        || (*context_.tearingDown)
        || (*context_.detailsPopulating)
        || !BlockEditorSourceController(context_.sourceContext()).hasEditor()) {
        return;
    }
    const BlockEditorSourceController source(context_.sourceContext());

    QString validationError;
    QString candidateLine;
    const bool buildOk = context_.buildUpdatedLine(&candidateLine, &validationError);

    if (statusLabel() == nullptr) {
        return;
    }
    if (!validationError.isEmpty()) {
        statusLabel()->setStyleSheet(QStringLiteral("color: #c0392b;"));
        statusLabel()->setText(
            QStringLiteral("%1 — %2").arg(*context_.baseStatusText, validationError));
        return;
    }
    statusLabel()->setStyleSheet(QString());
    statusLabel()->setText(*context_.baseStatusText);
}
}
