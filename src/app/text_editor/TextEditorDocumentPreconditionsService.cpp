#include "TextEditorDocumentPreconditionsService.h"

namespace TherionStudio
{
bool TextEditorDocumentPreconditionsService::canLoad(const LoadPreconditions &preconditions)
{
    return preconditions.hasEditor
        && preconditions.hasFilePath
        && preconditions.hasFileEncodingName
        && preconditions.hasFileEncodingLabel
        && preconditions.hasEncodingStatusNote
        && preconditions.hasLoadingFlag
        && preconditions.hasCurrentLineNumber
        && preconditions.hasCurrentColumnNumber
        && preconditions.hasHighlightedLineNumber
        && preconditions.hasCleanTextSnapshot
        && preconditions.hasCleanEncodingNameSnapshot
        && preconditions.hasDirtyFlag
        && preconditions.hasBlocksModeActiveFlag
        && preconditions.hasBlockDetailsSelectedLineNumber
        && preconditions.hasBlockDetailsSelectedKind;
}

bool TextEditorDocumentPreconditionsService::canSave(const SavePreconditions &preconditions,
                                                     const QString &missingPathErrorMessage,
                                                     QString *errorMessage)
{
    const bool hasRequiredReferences = preconditions.hasEditor
        && preconditions.hasFilePath
        && preconditions.hasFileEncodingName
        && preconditions.hasFileEncodingLabel
        && preconditions.hasEncodingStatusNote
        && preconditions.hasCleanTextSnapshot
        && preconditions.hasCleanEncodingNameSnapshot
        && preconditions.hasDirtyFlag;
    if (!hasRequiredReferences) {
        return false;
    }

    if (preconditions.filePath.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = missingPathErrorMessage;
        }
        return false;
    }

    return true;
}
}
