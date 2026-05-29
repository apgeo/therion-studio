#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorDocumentPreconditionsService final
{
public:
    struct LoadPreconditions
    {
        bool hasEditor = false;
        bool hasFilePath = false;
        bool hasFileEncodingName = false;
        bool hasFileEncodingLabel = false;
        bool hasEncodingStatusNote = false;
        bool hasLoadingFlag = false;
        bool hasCurrentLineNumber = false;
        bool hasCurrentColumnNumber = false;
        bool hasHighlightedLineNumber = false;
        bool hasCleanTextSnapshot = false;
        bool hasCleanEncodingNameSnapshot = false;
        bool hasDirtyFlag = false;
        bool hasBlocksModeActiveFlag = false;
        bool hasBlockDetailsSelectedLineNumber = false;
        bool hasBlockDetailsSelectedKind = false;
    };

    struct SavePreconditions
    {
        bool hasEditor = false;
        bool hasFilePath = false;
        bool hasFileEncodingName = false;
        bool hasFileEncodingLabel = false;
        bool hasEncodingStatusNote = false;
        bool hasCleanTextSnapshot = false;
        bool hasCleanEncodingNameSnapshot = false;
        bool hasDirtyFlag = false;
        QString filePath;
    };

    static bool canLoad(const LoadPreconditions &preconditions);

    static bool canSave(const SavePreconditions &preconditions,
                        const QString &missingPathErrorMessage,
                        QString *errorMessage);
};
}
