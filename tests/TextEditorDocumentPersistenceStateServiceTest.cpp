#include "../src/app/text_editor/TextEditorDocumentPersistenceStateService.h"

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

int runBuildLoadStateUpdateNormalizesEncodingTest()
{
    TextEditorDocumentPersistenceStateService::LoadStateInput input;
    input.filePath = QStringLiteral("/tmp/survey.th");
    input.textContents = QStringLiteral("survey\nendsurvey\n");
    input.loadedEncodingName = QString();
    input.loadedEncodingLabel = QString();
    input.cursorLineNumber = 4;
    input.cursorColumnNumber = 7;
    input.blocksModeActive = true;
    input.blocksModeSupportedForCurrentFile = false;
    input.openedEncodingStatusTemplate = QStringLiteral("Opened as %1.");

    const auto update = TextEditorDocumentPersistenceStateService::buildLoadStateUpdate(input);
    if (!expect(update.fileEncodingName == QStringLiteral("UTF-8"),
                "Load update should normalize empty encoding name to UTF-8.")) {
        return 1;
    }
    if (!expect(update.fileEncodingLabel == QStringLiteral("UTF-8"),
                "Load update should normalize empty encoding label to UTF-8.")) {
        return 1;
    }
    if (!expect(update.encodingStatusNote.isEmpty(),
                "Load update for UTF-8 should clear encoding status note.")) {
        return 1;
    }
    if (!expect(update.currentLineNumber == 4 && update.currentColumnNumber == 7,
                "Load update should keep cursor location.")) {
        return 1;
    }
    if (!expect(update.highlightedLineNumber == 4,
                "Load update should align highlighted line with current line.")) {
        return 1;
    }
    if (!expect(update.disableBlocksMode,
                "Load update should request blocks-mode disable when unsupported.")) {
        return 1;
    }
    if (!expect(update.cleanTextSnapshot == input.textContents,
                "Load update should preserve clean text snapshot.")) {
        return 1;
    }
    if (!expect(update.blockDetailsSelectedLineNumber == 0 && update.blockDetailsSelectedKind.isEmpty(),
                "Load update should reset block-details selection state.")) {
        return 1;
    }

    return 0;
}

int runBuildLoadStateUpdateNonUtf8StatusTest()
{
    TextEditorDocumentPersistenceStateService::LoadStateInput input;
    input.loadedEncodingName = QStringLiteral("cp1250");
    input.loadedEncodingLabel = QStringLiteral("Windows-1250");
    input.openedEncodingStatusTemplate = QStringLiteral("Opened as %1. Keep encoding.");

    const auto update = TextEditorDocumentPersistenceStateService::buildLoadStateUpdate(input);
    if (!expect(update.fileEncodingName == QStringLiteral("cp1250"),
                "Load update should keep explicit encoding name.")) {
        return 1;
    }
    if (!expect(update.fileEncodingLabel == QStringLiteral("Windows-1250"),
                "Load update should keep explicit encoding label.")) {
        return 1;
    }
    if (!expect(update.encodingStatusNote == QStringLiteral("Opened as Windows-1250. Keep encoding."),
                "Load update should format non-UTF-8 opened status note.")) {
        return 1;
    }

    return 0;
}

int runBuildSaveStateUpdateStatusTest()
{
    TextEditorDocumentPersistenceStateService::SaveStateInput utf8Input;
    utf8Input.textContents = QStringLiteral("abc");
    utf8Input.fileEncodingName = QStringLiteral("UTF-8");
    utf8Input.fileEncodingLabel = QStringLiteral("UTF-8");
    utf8Input.savedEncodingStatusTemplate = QStringLiteral("Saved using %1.");

    const auto utf8Update = TextEditorDocumentPersistenceStateService::buildSaveStateUpdate(utf8Input);
    if (!expect(utf8Update.encodingStatusNote.isEmpty(),
                "Save update should clear encoding note for UTF-8.")) {
        return 1;
    }
    if (!expect(utf8Update.cleanTextSnapshot == QStringLiteral("abc")
                && utf8Update.cleanEncodingNameSnapshot == QStringLiteral("UTF-8"),
                "Save update should refresh clean snapshots.")) {
        return 1;
    }

    TextEditorDocumentPersistenceStateService::SaveStateInput nonUtf8Input;
    nonUtf8Input.textContents = QStringLiteral("def");
    nonUtf8Input.fileEncodingName = QStringLiteral("ISO-8859-2");
    nonUtf8Input.fileEncodingLabel = QStringLiteral("ISO-8859-2");
    nonUtf8Input.savedEncodingStatusTemplate = QStringLiteral("Saved using %1 encoding.");

    const auto nonUtf8Update = TextEditorDocumentPersistenceStateService::buildSaveStateUpdate(nonUtf8Input);
    if (!expect(nonUtf8Update.encodingStatusNote == QStringLiteral("Saved using ISO-8859-2 encoding."),
                "Save update should format non-UTF-8 saved status note.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runBuildLoadStateUpdateNormalizesEncodingTest() != 0) {
        return 1;
    }
    if (runBuildLoadStateUpdateNonUtf8StatusTest() != 0) {
        return 1;
    }
    return runBuildSaveStateUpdateStatusTest();
}
