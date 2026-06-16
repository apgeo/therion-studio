#include "../src/app/text_editor/TextEditorDocumentIoService.h"
#include "../src/core/IFileSystem.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
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

class TextEditorDocumentIoServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void readsDocument();
    void writesDocument();
    void buildsStateInputs();
};

void TextEditorDocumentIoServiceTest::readsDocument()
{
    FakeFileSystem fileSystem;
    TextEditorDocumentIoService::ReadDocumentResult result;
    QString errorMessage;

    QVERIFY(TextEditorDocumentIoService::readDocument(fileSystem,
                                                      QStringLiteral("/tmp/survey.th"),
                                                      &errorMessage,
                                                      &result));
    QCOMPARE(fileSystem.lastReadPath, QStringLiteral("/tmp/survey.th"));
    QCOMPARE(result.contents, fileSystem.readContents);
    QCOMPARE(result.loadedEncodingName, fileSystem.readEncodingName);
    QCOMPARE(result.loadedEncodingLabel, fileSystem.readEncodingLabel);

    fileSystem.readSucceeds = false;
    result = {};
    errorMessage.clear();
    QVERIFY(!TextEditorDocumentIoService::readDocument(fileSystem,
                                                       QStringLiteral("/tmp/missing.th"),
                                                       &errorMessage,
                                                       &result));
    QCOMPARE(errorMessage, fileSystem.failureMessage);
}

void TextEditorDocumentIoServiceTest::writesDocument()
{
    FakeFileSystem fileSystem;
    QString errorMessage;

    QVERIFY(TextEditorDocumentIoService::writeDocument(fileSystem,
                                                       QStringLiteral("/tmp/saved.th"),
                                                       QStringLiteral("updated"),
                                                       QStringLiteral("UTF-8"),
                                                       &errorMessage));
    QCOMPARE(fileSystem.lastWritePath, QStringLiteral("/tmp/saved.th"));
    QCOMPARE(fileSystem.lastWriteContents, QStringLiteral("updated"));
    QCOMPARE(fileSystem.lastWriteEncoding, QStringLiteral("UTF-8"));

    fileSystem.writeSucceeds = false;
    errorMessage.clear();
    QVERIFY(!TextEditorDocumentIoService::writeDocument(fileSystem,
                                                        QStringLiteral("/tmp/fail.th"),
                                                        QStringLiteral("x"),
                                                        QStringLiteral("CP1250"),
                                                        &errorMessage));
    QCOMPARE(errorMessage, fileSystem.failureMessage);
}

void TextEditorDocumentIoServiceTest::buildsStateInputs()
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
    QCOMPARE(loadInput.filePath, loadRequest.filePath);
    QCOMPARE(loadInput.textContents, loadRequest.contents);
    QCOMPARE(loadInput.loadedEncodingName, loadRequest.loadedEncodingName);
    QCOMPARE(loadInput.loadedEncodingLabel, loadRequest.loadedEncodingLabel);
    QCOMPARE(loadInput.cursorLineNumber, loadRequest.cursorLineNumber);
    QCOMPARE(loadInput.cursorColumnNumber, loadRequest.cursorColumnNumber);
    QCOMPARE(loadInput.blocksModeActive, loadRequest.blocksModeActive);
    QCOMPARE(loadInput.blocksModeSupportedForCurrentFile, loadRequest.blocksModeSupportedForCurrentFile);
    QCOMPARE(loadInput.openedEncodingStatusTemplate, loadRequest.openedEncodingStatusTemplate);

    TextEditorDocumentIoService::BuildSaveStateInputRequest saveRequest;
    saveRequest.textContents = QStringLiteral("new text");
    saveRequest.fileEncodingName = QStringLiteral("UTF-8");
    saveRequest.fileEncodingLabel = QStringLiteral("UTF-8");
    saveRequest.savedEncodingStatusTemplate = QStringLiteral("Saved using %1");

    const auto saveInput = TextEditorDocumentIoService::buildSaveStateInput(saveRequest);
    QCOMPARE(saveInput.textContents, saveRequest.textContents);
    QCOMPARE(saveInput.fileEncodingName, saveRequest.fileEncodingName);
    QCOMPARE(saveInput.fileEncodingLabel, saveRequest.fileEncodingLabel);
    QCOMPARE(saveInput.savedEncodingStatusTemplate, saveRequest.savedEncodingStatusTemplate);
}
}

int runTextEditorDocumentIoServiceTest(int argc, char **argv)
{
    TextEditorDocumentIoServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TextEditorDocumentIoServiceTest.moc"
