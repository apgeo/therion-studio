#include "TextEditorTab.h"

#include "TextEditorEncodingController.h"

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildEncodingController()
{
    TextEditorEncodingContext encodingContext;
    encodingContext.dialogParent = this;
    encodingContext.fileEncodingName = &fileEncodingName_;
    encodingContext.fileEncodingLabel = &fileEncodingLabel_;
    encodingContext.encodingStatusNote = &encodingStatusNote_;
    encodingContext.isBlocksModeSupportedForCurrentFile = [this]() {
        return isBlocksModeSupportedForCurrentFile();
    };
    encodingContext.ensureEncodingRootDirectiveForBlocks = [this]() {
        return ensureEncodingRootDirectiveForBlocks();
    };
    encodingContext.applyDirtyStateFromCurrentState = [this]() {
        applyDirtyStateFromCurrentState();
    };
    encodingContext.refreshStatus = [this]() {
        refreshStatus();
    };

    encodingController_ = std::make_unique<TextEditorEncodingController>(std::move(encodingContext));
}
}
