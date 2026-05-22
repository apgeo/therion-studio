#include "TextEditorEncodingController.h"

#include <QCoreApplication>
#include <QMessageBox>
#include <QString>

#include <utility>

namespace TherionStudio
{
TextEditorEncodingController::TextEditorEncodingController(TextEditorEncodingContext context)
    : context_(std::move(context))
{
}

void TextEditorEncodingController::handleConvertToUtf8Triggered()
{
    if (context_.dialogParent == nullptr
        || context_.fileEncodingName == nullptr
        || context_.fileEncodingLabel == nullptr
        || context_.encodingStatusNote == nullptr) {
        return;
    }
    if (context_.fileEncodingName->compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return;
    }

    const auto answer = QMessageBox::question(context_.dialogParent,
                                              trText("Convert to UTF-8"),
                                              trText("Convert this document from %1 to UTF-8?\n\n"
                                                     "The file on disk is not changed until you save.")
                                                  .arg(*context_.fileEncodingLabel),
                                              QMessageBox::Yes | QMessageBox::No,
                                              QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    *context_.fileEncodingName = QStringLiteral("UTF-8");
    *context_.fileEncodingLabel = QStringLiteral("UTF-8");
    *context_.encodingStatusNote = trText("Converted to UTF-8 in memory. Save to write UTF-8 to disk.");
    if (context_.isBlocksModeSupportedForCurrentFile
        && context_.isBlocksModeSupportedForCurrentFile()
        && context_.ensureEncodingRootDirectiveForBlocks) {
        context_.ensureEncodingRootDirectiveForBlocks();
    }
    if (context_.applyDirtyStateFromCurrentState) {
        context_.applyDirtyStateFromCurrentState();
    }
    if (context_.refreshStatus) {
        context_.refreshStatus();
    }
}

QString TextEditorEncodingController::trText(const char *sourceText)
{
    return QCoreApplication::translate("TherionStudio::TextEditorTab", sourceText);
}
}
