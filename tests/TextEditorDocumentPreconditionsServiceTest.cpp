#include "../src/app/text_editor/TextEditorDocumentPreconditionsService.h"

#include <QCoreApplication>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

TextEditorDocumentPreconditionsService::LoadPreconditions validLoadPreconditions()
{
    TextEditorDocumentPreconditionsService::LoadPreconditions preconditions;
    preconditions.hasEditor = true;
    preconditions.hasFilePath = true;
    preconditions.hasFileEncodingName = true;
    preconditions.hasFileEncodingLabel = true;
    preconditions.hasEncodingStatusNote = true;
    preconditions.hasLoadingFlag = true;
    preconditions.hasCurrentLineNumber = true;
    preconditions.hasCurrentColumnNumber = true;
    preconditions.hasHighlightedLineNumber = true;
    preconditions.hasCleanTextSnapshot = true;
    preconditions.hasCleanEncodingNameSnapshot = true;
    preconditions.hasDirtyFlag = true;
    preconditions.hasBlocksModeActiveFlag = true;
    preconditions.hasBlockDetailsSelectedLineNumber = true;
    preconditions.hasBlockDetailsSelectedKind = true;
    return preconditions;
}

TextEditorDocumentPreconditionsService::SavePreconditions validSavePreconditions()
{
    TextEditorDocumentPreconditionsService::SavePreconditions preconditions;
    preconditions.hasEditor = true;
    preconditions.hasFilePath = true;
    preconditions.hasFileEncodingName = true;
    preconditions.hasFileEncodingLabel = true;
    preconditions.hasEncodingStatusNote = true;
    preconditions.hasCleanTextSnapshot = true;
    preconditions.hasCleanEncodingNameSnapshot = true;
    preconditions.hasDirtyFlag = true;
    preconditions.filePath = QStringLiteral("/tmp/survey.th");
    return preconditions;
}

int runCanLoadChecksAllRequiredPointersTest()
{
    const auto valid = validLoadPreconditions();
    if (!expect(TextEditorDocumentPreconditionsService::canLoad(valid),
                "canLoad should pass for complete load preconditions.")) {
        return 1;
    }

    auto missingEditor = valid;
    missingEditor.hasEditor = false;
    if (!expect(!TextEditorDocumentPreconditionsService::canLoad(missingEditor),
                "canLoad should fail when editor pointer is missing.")) {
        return 1;
    }

    auto missingBlockSelectionKind = valid;
    missingBlockSelectionKind.hasBlockDetailsSelectedKind = false;
    if (!expect(!TextEditorDocumentPreconditionsService::canLoad(missingBlockSelectionKind),
                "canLoad should fail when block-details selected kind pointer is missing.")) {
        return 1;
    }

    return 0;
}

int runCanSaveChecksMissingPathMessageTest()
{
    const auto valid = validSavePreconditions();
    QString errorMessage;
    if (!expect(TextEditorDocumentPreconditionsService::canSave(valid,
                                                                 QStringLiteral("missing path"),
                                                                 &errorMessage),
                "canSave should pass for complete save preconditions and file path.")) {
        return 1;
    }

    auto missingCleanSnapshot = valid;
    missingCleanSnapshot.hasCleanTextSnapshot = false;
    errorMessage.clear();
    if (!expect(!TextEditorDocumentPreconditionsService::canSave(missingCleanSnapshot,
                                                                  QStringLiteral("missing path"),
                                                                  &errorMessage),
                "canSave should fail when clean snapshot pointer is missing.")) {
        return 1;
    }
    if (!expect(errorMessage.isEmpty(),
                "canSave should not set error message for missing pointer preconditions.")) {
        return 1;
    }

    auto missingPath = valid;
    missingPath.filePath.clear();
    errorMessage.clear();
    if (!expect(!TextEditorDocumentPreconditionsService::canSave(missingPath,
                                                                  QStringLiteral("missing path"),
                                                                  &errorMessage),
                "canSave should fail when document path is empty.")) {
        return 1;
    }
    if (!expect(errorMessage == QStringLiteral("missing path"),
                "canSave should surface missing-path message for empty path.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runCanLoadChecksAllRequiredPointersTest() != 0) {
        return 1;
    }
    return runCanSaveChecksMissingPathMessageTest();
}
