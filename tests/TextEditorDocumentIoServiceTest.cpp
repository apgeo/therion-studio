#include "../src/app/text_editor/TextEditorDocumentIoService.h"
#include "../src/core/IFileSystem.h"

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

class FakeFileSystem final : public IFileSystem
{
public:
    bool readSucceeds = true;
    bool writeSucceeds = true;
    QString readContents = QStringLiteral("survey\nendsurvey\n");
    QString readEncodingName = QStringLiteral("UTF-8");
    QString readEncodingLabel = QStringLiteral("UTF-8");
    QString failureMessage = QStringLiteral("io failure");
    QString lastReadPath;
    QString lastWritePath;
    QString lastWriteContents;
    QString lastWriteEncoding;

    bool readTextFile(const QString &filePath,
                      QString *contents,
                      QString *encodingName,
                      QString *encodingLabel,
                      QString *errorMessage) override
    {
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

int runReadDocumentTest()
{
    FakeFileSystem fileSystem;
    TextEditorDocumentIoService::ReadDocumentResult result;
    QString errorMessage;

    if (!expect(TextEditorDocumentIoService::readDocument(fileSystem,
                                                          QStringLiteral("/tmp/survey.th"),
                                                          &errorMessage,
                                                          &result),
                "readDocument should pass on successful filesystem read.")) {
        return 1;
    }
    if (!expect(fileSystem.lastReadPath == QStringLiteral("/tmp/survey.th"),
                "readDocument should pass file path to filesystem.")) {
        return 1;
    }
    if (!expect(result.contents == fileSystem.readContents,
                "readDocument should expose loaded contents.")) {
        return 1;
    }
    if (!expect(result.loadedEncodingName == fileSystem.readEncodingName,
                "readDocument should expose loaded encoding name.")) {
        return 1;
    }
    if (!expect(result.loadedEncodingLabel == fileSystem.readEncodingLabel,
                "readDocument should expose loaded encoding label.")) {
        return 1;
    }

    fileSystem.readSucceeds = false;
    result = {};
    errorMessage.clear();
    if (!expect(!TextEditorDocumentIoService::readDocument(fileSystem,
                                                           QStringLiteral("/tmp/missing.th"),
                                                           &errorMessage,
                                                           &result),
                "readDocument should fail when filesystem read fails.")) {
        return 1;
    }
    if (!expect(errorMessage == fileSystem.failureMessage,
                "readDocument should propagate filesystem read error message.")) {
        return 1;
    }

    return 0;
}

int runWriteDocumentTest()
{
    FakeFileSystem fileSystem;
    QString errorMessage;

    if (!expect(TextEditorDocumentIoService::writeDocument(fileSystem,
                                                           QStringLiteral("/tmp/saved.th"),
                                                           QStringLiteral("updated"),
                                                           QStringLiteral("UTF-8"),
                                                           &errorMessage),
                "writeDocument should pass on successful filesystem write.")) {
        return 1;
    }
    if (!expect(fileSystem.lastWritePath == QStringLiteral("/tmp/saved.th")
                && fileSystem.lastWriteContents == QStringLiteral("updated")
                && fileSystem.lastWriteEncoding == QStringLiteral("UTF-8"),
                "writeDocument should pass path/text/encoding to filesystem.")) {
        return 1;
    }

    fileSystem.writeSucceeds = false;
    errorMessage.clear();
    if (!expect(!TextEditorDocumentIoService::writeDocument(fileSystem,
                                                            QStringLiteral("/tmp/fail.th"),
                                                            QStringLiteral("x"),
                                                            QStringLiteral("CP1250"),
                                                            &errorMessage),
                "writeDocument should fail when filesystem write fails.")) {
        return 1;
    }
    if (!expect(errorMessage == fileSystem.failureMessage,
                "writeDocument should propagate filesystem write error message.")) {
        return 1;
    }

    return 0;
}

int runBuildStateInputsTest()
{
    TextEditorDocumentIoService::BuildLoadStateInputRequest loadRequest;
    loadRequest.filePath = QStringLiteral("/tmp/survey.th");
    loadRequest.contents = QStringLiteral("survey");
    loadRequest.loadedEncodingName = QStringLiteral("CP1250");
    loadRequest.loadedEncodingLabel = QStringLiteral("Windows-1250");
    loadRequest.cursorLineNumber = 7;
    loadRequest.cursorColumnNumber = 11;
    loadRequest.blocksModeActive = true;
    loadRequest.blocksModeSupportedForCurrentFile = false;
    loadRequest.openedEncodingStatusTemplate = QStringLiteral("Opened as %1");

    const auto loadInput = TextEditorDocumentIoService::buildLoadStateInput(loadRequest);
    if (!expect(loadInput.filePath == loadRequest.filePath
                && loadInput.textContents == loadRequest.contents
                && loadInput.loadedEncodingName == loadRequest.loadedEncodingName
                && loadInput.loadedEncodingLabel == loadRequest.loadedEncodingLabel
                && loadInput.cursorLineNumber == loadRequest.cursorLineNumber
                && loadInput.cursorColumnNumber == loadRequest.cursorColumnNumber
                && loadInput.blocksModeActive == loadRequest.blocksModeActive
                && loadInput.blocksModeSupportedForCurrentFile == loadRequest.blocksModeSupportedForCurrentFile
                && loadInput.openedEncodingStatusTemplate == loadRequest.openedEncodingStatusTemplate,
                "buildLoadStateInput should map all fields.")) {
        return 1;
    }

    TextEditorDocumentIoService::BuildSaveStateInputRequest saveRequest;
    saveRequest.textContents = QStringLiteral("new text");
    saveRequest.fileEncodingName = QStringLiteral("UTF-8");
    saveRequest.fileEncodingLabel = QStringLiteral("UTF-8");
    saveRequest.savedEncodingStatusTemplate = QStringLiteral("Saved using %1");

    const auto saveInput = TextEditorDocumentIoService::buildSaveStateInput(saveRequest);
    if (!expect(saveInput.textContents == saveRequest.textContents
                && saveInput.fileEncodingName == saveRequest.fileEncodingName
                && saveInput.fileEncodingLabel == saveRequest.fileEncodingLabel
                && saveInput.savedEncodingStatusTemplate == saveRequest.savedEncodingStatusTemplate,
                "buildSaveStateInput should map all fields.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runReadDocumentTest() != 0) {
        return 1;
    }
    if (runWriteDocumentTest() != 0) {
        return 1;
    }
    return runBuildStateInputsTest();
}
