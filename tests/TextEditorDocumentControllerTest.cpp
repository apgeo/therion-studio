#include "../src/app/text_editor/TextEditorDocumentController.h"
#include "../src/core/IFileSystem.h"

#include <QApplication>
#include <QPlainTextEdit>
#include <QString>

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

class FakeFileSystem final : public IFileSystem
{
public:
    bool readSucceeds = true;
    bool writeSucceeds = true;
    QString readContents = QStringLiteral("survey test\nendsurvey\n");
    QString readEncodingName = QStringLiteral("UTF-8");
    QString readEncodingLabel = QStringLiteral("UTF-8");
    QString failureMessage = QStringLiteral("simulated filesystem failure");
    QString lastReadPath;
    QString lastWritePath;
    QString lastWriteContents;
    QString lastWriteEncoding;
    int readCalls = 0;
    int writeCalls = 0;

    bool readTextFile(const QString &filePath,
                      QString *contents,
                      QString *encodingName,
                      QString *encodingLabel,
                      QString *errorMessage) override
    {
        ++readCalls;
        lastReadPath = filePath;
        if (!readSucceeds) {
            if (errorMessage != nullptr) {
                *errorMessage = failureMessage;
            }
            return false;
        }
        if (contents != nullptr) {
            *contents = readContents;
        }
        if (encodingName != nullptr) {
            *encodingName = readEncodingName;
        }
        if (encodingLabel != nullptr) {
            *encodingLabel = readEncodingLabel;
        }
        return true;
    }

    bool writeTextFile(const QString &filePath,
                       const QString &contents,
                       const QString &encodingName,
                       QString *errorMessage) override
    {
        ++writeCalls;
        lastWritePath = filePath;
        lastWriteContents = contents;
        lastWriteEncoding = encodingName;
        if (!writeSucceeds) {
            if (errorMessage != nullptr) {
                *errorMessage = failureMessage;
            }
            return false;
        }
        return true;
    }

    bool readUtf8TextFile(const QString &, QString *, QString *) override
    {
        return false;
    }

    bool writeUtf8TextFile(const QString &, const QString &, QString *) override
    {
        return false;
    }
};

struct ControllerFixture
{
    QPlainTextEdit editor;
    QString filePath;
    QString projectRootPath;
    QString fileEncodingName = QStringLiteral("UTF-8");
    QString fileEncodingLabel = QStringLiteral("UTF-8");
    QString encodingStatusNote;
    QString cleanTextSnapshot;
    QString cleanEncodingNameSnapshot;
    QString blockDetailsSelectedKind = QStringLiteral("line");
    bool loading = false;
    bool dirty = true;
    bool blocksModeActive = false;
    int currentLineNumber = 42;
    int currentColumnNumber = 24;
    int highlightedLineNumber = 7;
    int blockDetailsSelectedLineNumber = 10;
    int refreshTitleCount = 0;
    int dirtyStateChangedCount = 0;
    bool lastDirtyState = true;

    TextEditorDocumentContext makeContext()
    {
        TextEditorDocumentContext context;
        context.editor = &editor;
        context.filePath = &filePath;
        context.projectRootPath = &projectRootPath;
        context.fileEncodingName = &fileEncodingName;
        context.fileEncodingLabel = &fileEncodingLabel;
        context.encodingStatusNote = &encodingStatusNote;
        context.cleanTextSnapshot = &cleanTextSnapshot;
        context.cleanEncodingNameSnapshot = &cleanEncodingNameSnapshot;
        context.blockDetailsSelectedKind = &blockDetailsSelectedKind;
        context.loading = &loading;
        context.dirty = &dirty;
        context.blocksModeActive = &blocksModeActive;
        context.currentLineNumber = &currentLineNumber;
        context.currentColumnNumber = &currentColumnNumber;
        context.highlightedLineNumber = &highlightedLineNumber;
        context.blockDetailsSelectedLineNumber = &blockDetailsSelectedLineNumber;
        context.translate = [](const char *text) { return QString::fromUtf8(text); };
        context.refreshTitle = [this] { ++refreshTitleCount; };
        context.dirtyStateChanged = [this](bool dirtyState) {
            ++dirtyStateChangedCount;
            lastDirtyState = dirtyState;
        };
        return context;
    }
};

int runLoadUsesInjectedFileSystemTest()
{
    FakeFileSystem fileSystem;
    fileSystem.readContents = QStringLiteral("survey injected\nendsurvey\n");
    ControllerFixture fixture;
    TextEditorDocumentController controller(fileSystem, fixture.makeContext());

    QString errorMessage;
    if (!expect(controller.loadFile(QStringLiteral("/tmp/injected.th"), &errorMessage),
                "loadFile should succeed through the injected filesystem.")) {
        return 1;
    }
    if (!expect(fileSystem.readCalls == 1, "loadFile should call the injected filesystem exactly once.")) {
        return 1;
    }
    if (!expect(fileSystem.lastReadPath == QStringLiteral("/tmp/injected.th"),
                "loadFile should pass the requested path to the injected filesystem.")) {
        return 1;
    }
    if (!expect(fixture.editor.toPlainText() == fileSystem.readContents,
                "loadFile should populate the editor from filesystem contents.")) {
        return 1;
    }
    if (!expect(fixture.filePath == QStringLiteral("/tmp/injected.th"),
                "loadFile should update the document file path.")) {
        return 1;
    }
    if (!expect(!fixture.dirty && !fixture.editor.document()->isModified(),
                "loadFile should reset dirty state and document modified state.")) {
        return 1;
    }
    if (!expect(fixture.cleanTextSnapshot == fileSystem.readContents,
                "loadFile should refresh the clean text snapshot.")) {
        return 1;
    }
    if (!expect(fixture.blockDetailsSelectedLineNumber == 0 && fixture.blockDetailsSelectedKind.isEmpty(),
                "loadFile should clear block-details selection state.")) {
        return 1;
    }
    if (!expect(fixture.dirtyStateChangedCount == 1 && !fixture.lastDirtyState,
                "loadFile should notify dirty state reset.")) {
        return 1;
    }
    return 0;
}

int runSaveUsesInjectedFileSystemTest()
{
    FakeFileSystem fileSystem;
    ControllerFixture fixture;
    fixture.filePath = QStringLiteral("/tmp/saved.th");
    fixture.fileEncodingName = QStringLiteral("UTF-8");
    fixture.editor.setPlainText(QStringLiteral("survey changed\nendsurvey\n"));
    fixture.editor.document()->setModified(true);
    fixture.dirty = true;
    TextEditorDocumentController controller(fileSystem, fixture.makeContext());

    QString errorMessage;
    if (!expect(controller.save(&errorMessage), "save should succeed through the injected filesystem.")) {
        return 1;
    }
    if (!expect(fileSystem.writeCalls == 1, "save should call the injected filesystem exactly once.")) {
        return 1;
    }
    if (!expect(fileSystem.lastWritePath == fixture.filePath,
                "save should pass the document path to the injected filesystem.")) {
        return 1;
    }
    if (!expect(fileSystem.lastWriteContents == QStringLiteral("survey changed\nendsurvey\n"),
                "save should pass current editor text to the injected filesystem.")) {
        return 1;
    }
    if (!expect(fileSystem.lastWriteEncoding == QStringLiteral("UTF-8"),
                "save should preserve the current file encoding.")) {
        return 1;
    }
    if (!expect(!fixture.dirty && !fixture.editor.document()->isModified(),
                "save should reset dirty state and document modified state.")) {
        return 1;
    }
    if (!expect(fixture.cleanTextSnapshot == fileSystem.lastWriteContents,
                "save should refresh the clean text snapshot.")) {
        return 1;
    }
    if (!expect(fixture.dirtyStateChangedCount == 1 && !fixture.lastDirtyState,
                "save should notify dirty state reset.")) {
        return 1;
    }
    return 0;
}

int runFailureDoesNotMutateLoadedStateTest()
{
    FakeFileSystem fileSystem;
    fileSystem.readSucceeds = false;
    ControllerFixture fixture;
    fixture.filePath = QStringLiteral("/tmp/original.th");
    TextEditorDocumentController controller(fileSystem, fixture.makeContext());

    QString errorMessage;
    if (!expect(!controller.loadFile(QStringLiteral("/tmp/missing.th"), &errorMessage),
                "loadFile should fail when the injected filesystem fails.")) {
        return 1;
    }
    if (!expect(errorMessage == fileSystem.failureMessage,
                "loadFile should propagate the injected filesystem error message.")) {
        return 1;
    }
    if (!expect(fixture.filePath == QStringLiteral("/tmp/original.th"),
                "failed loadFile should not replace the current document path.")) {
        return 1;
    }
    if (!expect(fixture.editor.toPlainText().isEmpty(),
                "failed loadFile should not populate editor contents.")) {
        return 1;
    }
    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    (void)app;

    if (const int result = runLoadUsesInjectedFileSystemTest(); result != 0) {
        return result;
    }
    if (const int result = runSaveUsesInjectedFileSystemTest(); result != 0) {
        return result;
    }
    return runFailureDoesNotMutateLoadedStateTest();
}
