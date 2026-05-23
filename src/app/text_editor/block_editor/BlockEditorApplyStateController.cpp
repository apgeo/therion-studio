#include "BlockEditorApplyStateController.h"

#include <QLabel>
#include <QPushButton>

#include <utility>

namespace TherionStudio
{
BlockEditorApplyStateController::BlockEditorApplyStateController(BlockEditorApplyStateContext context)
    : context_(std::move(context))
{
}

QPushButton *BlockEditorApplyStateController::applyButton() const
{
    return context_.applyButton != nullptr ? *context_.applyButton : nullptr;
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
        || applyButton() == nullptr
        || !BlockEditorSourceController(context_.sourceContext()).hasEditor()) {
        return;
    }
    const BlockEditorSourceController source(context_.sourceContext());

    QString validationError;
    QString candidateLine;
    const bool buildOk = context_.buildUpdatedLine(&candidateLine, &validationError);

    QString currentLine;
    bool hasCurrentLine = false;
    if ((*context_.selectedLineNumber) > 0) {
        QStringList lines = source.normalizedLines();
        if ((*context_.selectedLineNumber) <= lines.size()) {
            currentLine = lines.at((*context_.selectedLineNumber) - 1);
            hasCurrentLine = true;
        }
    }

    const bool hasChanges = buildOk && hasCurrentLine && candidateLine != currentLine;
    applyButton()->setEnabled(hasChanges);

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
