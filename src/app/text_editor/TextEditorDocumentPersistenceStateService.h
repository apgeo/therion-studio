#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorDocumentPersistenceStateService final
{
public:
    struct LoadStateInput
    {
        QString filePath;
        QString textContents;
        QString loadedEncodingName;
        QString loadedEncodingLabel;
        int cursorLineNumber = 1;
        int cursorColumnNumber = 1;
        bool blocksModeActive = false;
        bool blocksModeSupportedForCurrentFile = true;
        QString openedEncodingStatusTemplate;
    };

    struct LoadStateUpdate
    {
        QString filePath;
        QString fileEncodingName;
        QString fileEncodingLabel;
        QString encodingStatusNote;
        QString cleanTextSnapshot;
        QString cleanEncodingNameSnapshot;
        int currentLineNumber = 1;
        int currentColumnNumber = 1;
        int highlightedLineNumber = 1;
        int blockDetailsSelectedLineNumber = 0;
        QString blockDetailsSelectedKind;
        bool dirty = false;
        bool disableBlocksMode = false;
    };

    struct SaveStateInput
    {
        QString textContents;
        QString fileEncodingName;
        QString fileEncodingLabel;
        QString savedEncodingStatusTemplate;
    };

    struct SaveStateUpdate
    {
        QString cleanTextSnapshot;
        QString cleanEncodingNameSnapshot;
        QString encodingStatusNote;
        bool dirty = false;
    };

    static LoadStateUpdate buildLoadStateUpdate(const LoadStateInput &input);
    static SaveStateUpdate buildSaveStateUpdate(const SaveStateInput &input);
};
}
