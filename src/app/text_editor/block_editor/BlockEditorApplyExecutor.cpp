#include "BlockEditorApplyExecutor.h"

#include "../TextEditorTab.h"

#include <QMessageBox>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>

namespace TherionStudio
{
BlockEditorApplyExecutor::BlockEditorApplyExecutor(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorApplyExecutor::applyChanges()
{
    if (owner_ == nullptr || owner_->tearingDown_) {
        return;
    }

    QString updatedLine;
    QString validationError;
    if (!owner_->buildUpdatedLineFromBlockDetails(&updatedLine, &validationError)) {
        if (!validationError.isEmpty()) {
            QMessageBox::warning(owner_, TextEditorTab::tr("Configure Block"), validationError);
        }
        return;
    }

    const QTextBlock targetBlock = owner_->editor_->document()->findBlockByLineNumber(owner_->blockDetailsSelectedLineNumber_ - 1);
    if (!targetBlock.isValid()) {
        return;
    }

    QTextCursor editCursor(targetBlock);
    editCursor.beginEditBlock();
    editCursor.movePosition(QTextCursor::StartOfBlock);
    editCursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    editCursor.insertText(updatedLine);
    editCursor.endEditBlock();
    owner_->editor_->setTextCursor(editCursor);

    owner_->selectBlockInCanvasAndDetails(owner_->blockDetailsSelectedLineNumber_);
    owner_->refreshBlockDetailsApplyState();
}
}
