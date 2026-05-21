#include "TextEditorEncodingController.h"

#include "TextEditorTab.h"

#include <QMessageBox>

namespace TherionStudio
{
TextEditorEncodingController::TextEditorEncodingController(TextEditorTab *owner)
    : owner_(owner)
{
}

void TextEditorEncodingController::handleConvertToUtf8Triggered()
{
    if (owner_ == nullptr) {
        return;
    }
    if (owner_->fileEncodingName_.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return;
    }

    const auto answer = QMessageBox::question(owner_,
                                              TextEditorTab::tr("Convert to UTF-8"),
                                              TextEditorTab::tr("Convert this document from %1 to UTF-8?\n\n"
                                                                "The file on disk is not changed until you save.")
                                                  .arg(owner_->fileEncodingLabel_),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    owner_->fileEncodingName_ = QStringLiteral("UTF-8");
    owner_->fileEncodingLabel_ = QStringLiteral("UTF-8");
    owner_->encodingStatusNote_ = TextEditorTab::tr("Converted to UTF-8 in memory. Save to write UTF-8 to disk.");
    if (owner_->isBlocksModeSupportedForCurrentFile()) {
        owner_->ensureEncodingRootDirectiveForBlocks();
    }
    owner_->applyDirtyStateFromCurrentState();
    owner_->refreshStatus();
}
}
