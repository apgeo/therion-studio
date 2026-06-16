#include "../src/app/text_editor/TextEditorDocumentPersistenceStateService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TextEditorDocumentPersistenceStateServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void normalizesEncodingOnLoadStateUpdate();
    void formatsNonUtf8LoadStatus();
    void formatsSaveStateStatus();
};

void TextEditorDocumentPersistenceStateServiceTest::normalizesEncodingOnLoadStateUpdate()
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
    QCOMPARE(update.fileEncodingName, QStringLiteral("UTF-8"));
    QCOMPARE(update.fileEncodingLabel, QStringLiteral("UTF-8"));
    QVERIFY(update.encodingStatusNote.isEmpty());
    QCOMPARE(update.currentLineNumber, 4);
    QCOMPARE(update.currentColumnNumber, 7);
    QCOMPARE(update.highlightedLineNumber, 4);
    QVERIFY(update.disableBlocksMode);
    QCOMPARE(update.cleanTextSnapshot, input.textContents);
    QCOMPARE(update.blockDetailsSelectedLineNumber, 0);
    QVERIFY(update.blockDetailsSelectedKind.isEmpty());
}

void TextEditorDocumentPersistenceStateServiceTest::formatsNonUtf8LoadStatus()
{
    TextEditorDocumentPersistenceStateService::LoadStateInput input;
    input.loadedEncodingName = QStringLiteral("cp1250");
    input.loadedEncodingLabel = QStringLiteral("Windows-1250");
    input.openedEncodingStatusTemplate = QStringLiteral("Opened as %1. Keep encoding.");

    const auto update = TextEditorDocumentPersistenceStateService::buildLoadStateUpdate(input);
    QCOMPARE(update.fileEncodingName, QStringLiteral("cp1250"));
    QCOMPARE(update.fileEncodingLabel, QStringLiteral("Windows-1250"));
    QCOMPARE(update.encodingStatusNote, QStringLiteral("Opened as Windows-1250. Keep encoding."));
}

void TextEditorDocumentPersistenceStateServiceTest::formatsSaveStateStatus()
{
    TextEditorDocumentPersistenceStateService::SaveStateInput utf8Input;
    utf8Input.textContents = QStringLiteral("abc");
    utf8Input.fileEncodingName = QStringLiteral("UTF-8");
    utf8Input.fileEncodingLabel = QStringLiteral("UTF-8");
    utf8Input.savedEncodingStatusTemplate = QStringLiteral("Saved using %1.");

    const auto utf8Update = TextEditorDocumentPersistenceStateService::buildSaveStateUpdate(utf8Input);
    QVERIFY(utf8Update.encodingStatusNote.isEmpty());
    QCOMPARE(utf8Update.cleanTextSnapshot, QStringLiteral("abc"));
    QCOMPARE(utf8Update.cleanEncodingNameSnapshot, QStringLiteral("UTF-8"));

    TextEditorDocumentPersistenceStateService::SaveStateInput nonUtf8Input;
    nonUtf8Input.textContents = QStringLiteral("def");
    nonUtf8Input.fileEncodingName = QStringLiteral("ISO-8859-2");
    nonUtf8Input.fileEncodingLabel = QStringLiteral("ISO-8859-2");
    nonUtf8Input.savedEncodingStatusTemplate = QStringLiteral("Saved using %1 encoding.");

    const auto nonUtf8Update = TextEditorDocumentPersistenceStateService::buildSaveStateUpdate(nonUtf8Input);
    QCOMPARE(nonUtf8Update.encodingStatusNote, QStringLiteral("Saved using ISO-8859-2 encoding."));
}
}

int runTextEditorDocumentPersistenceStateServiceTest(int argc, char **argv)
{
    TextEditorDocumentPersistenceStateServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TextEditorDocumentPersistenceStateServiceTest.moc"
