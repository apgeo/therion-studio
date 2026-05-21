#include "BlockEditorApplyExecutor.h"

#include "BlockEditorSourceController.h"
#include "../TextEditorTab.h"

#include <QMessageBox>

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

    const BlockEditorSourceController source(owner_);
    if (!source.replaceLine(owner_->blockDetailsSelectedLineNumber_, updatedLine)) {
        return;
    }

    owner_->selectBlockInCanvasAndDetails(owner_->blockDetailsSelectedLineNumber_);
    owner_->refreshBlockDetailsApplyState();
}
}
