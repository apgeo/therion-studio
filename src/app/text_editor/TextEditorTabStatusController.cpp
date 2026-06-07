#include "TextEditorTab.h"

#include "TextEditorStatusController.h"

#include <utility>

namespace TherionStudio
{
void TextEditorTab::buildStatusController()
{
    TextEditorStatusContext statusContext;
    statusContext.editor = editor_;
    statusContext.encodingNoteLabel = encodingNoteLabel_;
    statusContext.convertEncodingButton = convertEncodingButton_;
    statusContext.statusRow = statusRow_;
    statusContext.filePath = &filePath_;
    statusContext.untitledDisplayName = &untitledDisplayName_;
    statusContext.projectRootPath = &projectRootPath_;
    statusContext.fileEncodingName = &fileEncodingName_;
    statusContext.fileEncodingLabel = &fileEncodingLabel_;
    statusContext.cleanTextSnapshot = &cleanTextSnapshot_;
    statusContext.cleanEncodingNameSnapshot = &cleanEncodingNameSnapshot_;
    statusContext.encodingStatusNote = &encodingStatusNote_;
    statusContext.dirty = &dirty_;
    statusContext.inlineStatusRequestedVisible = &inlineStatusRequestedVisible_;
    statusContext.titleChanged = [this]() {
        emit titleChanged();
    };
    statusContext.dirtyStateChanged = [this](bool dirty) {
        emit dirtyStateChanged(dirty);
    };

    statusController_ = std::make_unique<TextEditorStatusController>(std::move(statusContext));
}
}
