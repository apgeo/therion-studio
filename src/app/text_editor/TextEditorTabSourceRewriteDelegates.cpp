#include "TextEditorTab.h"

#include "TextEditorSourceRewriteController.h"

namespace TherionStudio
{
void TextEditorTab::applySourceSnapshotForTransaction(const QString &contents)
{
    if (sourceRewriteController_ != nullptr) {
        sourceRewriteController_->applySourceSnapshotForTransaction(contents);
    }
}

bool TextEditorTab::replaceTextForSystemNormalization(const QString &contents)
{
    if (sourceRewriteController_ == nullptr) {
        return false;
    }

    return sourceRewriteController_->replaceTextForSystemNormalization(contents);
}
}
