#include "TextEditorDocumentPersistenceStateService.h"

namespace
{
bool isUtf8Encoding(const QString &encodingName)
{
    return encodingName.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0;
}

QString normalizeEncodingName(const QString &encodingName)
{
    const QString normalized = encodingName.trimmed();
    return normalized.isEmpty() ? QStringLiteral("UTF-8") : normalized;
}

QString normalizeEncodingLabel(const QString &encodingLabel)
{
    return encodingLabel.isEmpty() ? QStringLiteral("UTF-8") : encodingLabel;
}
}

namespace TherionStudio
{
TextEditorDocumentPersistenceStateService::LoadStateUpdate
TextEditorDocumentPersistenceStateService::buildLoadStateUpdate(const LoadStateInput &input)
{
    LoadStateUpdate update;
    update.filePath = input.filePath;
    update.fileEncodingName = normalizeEncodingName(input.loadedEncodingName);
    update.fileEncodingLabel = normalizeEncodingLabel(input.loadedEncodingLabel);

    if (isUtf8Encoding(update.fileEncodingName)) {
        update.encodingStatusNote.clear();
    } else if (!input.openedEncodingStatusTemplate.isEmpty()) {
        update.encodingStatusNote = input.openedEncodingStatusTemplate.arg(update.fileEncodingLabel);
    }

    update.cleanTextSnapshot = input.textContents;
    update.cleanEncodingNameSnapshot = update.fileEncodingName;
    update.currentLineNumber = input.cursorLineNumber;
    update.currentColumnNumber = input.cursorColumnNumber;
    update.highlightedLineNumber = input.cursorLineNumber;
    update.blockDetailsSelectedLineNumber = 0;
    update.blockDetailsSelectedKind.clear();
    update.dirty = false;
    update.disableBlocksMode = input.blocksModeActive && !input.blocksModeSupportedForCurrentFile;
    return update;
}

TextEditorDocumentPersistenceStateService::SaveStateUpdate
TextEditorDocumentPersistenceStateService::buildSaveStateUpdate(const SaveStateInput &input)
{
    SaveStateUpdate update;
    update.cleanTextSnapshot = input.textContents;
    update.cleanEncodingNameSnapshot = input.fileEncodingName;

    if (isUtf8Encoding(input.fileEncodingName)) {
        update.encodingStatusNote.clear();
    } else if (!input.savedEncodingStatusTemplate.isEmpty()) {
        update.encodingStatusNote = input.savedEncodingStatusTemplate.arg(input.fileEncodingLabel);
    }

    update.dirty = false;
    return update;
}
}
