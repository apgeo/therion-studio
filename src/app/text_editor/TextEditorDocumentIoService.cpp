#include "TextEditorDocumentIoService.h"

#include "../../core/IFileSystem.h"

namespace TherionStudio
{
bool TextEditorDocumentIoService::readDocument(IFileSystem &fileSystem,
                                               const QString &filePath,
                                               QString *errorMessage,
                                               ReadDocumentResult *result)
{
    if (result == nullptr) {
        return false;
    }

    ReadDocumentResult payload;
    payload.loadedEncodingName = QStringLiteral("UTF-8");
    if (!fileSystem.readTextFile(filePath,
                                 &payload.contents,
                                 &payload.loadedEncodingName,
                                 &payload.loadedEncodingLabel,
                                 errorMessage)) {
        return false;
    }

    *result = payload;
    return true;
}

bool TextEditorDocumentIoService::writeDocument(IFileSystem &fileSystem,
                                                const QString &filePath,
                                                const QString &textContents,
                                                const QString &fileEncodingName,
                                                QString *errorMessage)
{
    return fileSystem.writeTextFile(filePath,
                                    textContents,
                                    fileEncodingName,
                                    errorMessage);
}

TextEditorDocumentPersistenceStateService::LoadStateInput
TextEditorDocumentIoService::buildLoadStateInput(const BuildLoadStateInputRequest &request)
{
    TextEditorDocumentPersistenceStateService::LoadStateInput input;
    input.filePath = request.filePath;
    input.textContents = request.contents;
    input.loadedEncodingName = request.loadedEncodingName;
    input.loadedEncodingLabel = request.loadedEncodingLabel;
    input.cursorLineNumber = request.cursorLineNumber;
    input.cursorColumnNumber = request.cursorColumnNumber;
    input.blocksModeActive = request.blocksModeActive;
    input.blocksModeSupportedForCurrentFile = request.blocksModeSupportedForCurrentFile;
    input.openedEncodingStatusTemplate = request.openedEncodingStatusTemplate;
    return input;
}

TextEditorDocumentPersistenceStateService::SaveStateInput
TextEditorDocumentIoService::buildSaveStateInput(const BuildSaveStateInputRequest &request)
{
    TextEditorDocumentPersistenceStateService::SaveStateInput input;
    input.textContents = request.textContents;
    input.fileEncodingName = request.fileEncodingName;
    input.fileEncodingLabel = request.fileEncodingLabel;
    input.savedEncodingStatusTemplate = request.savedEncodingStatusTemplate;
    return input;
}
}
