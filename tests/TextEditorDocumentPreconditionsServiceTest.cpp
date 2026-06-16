#include "../src/app/text_editor/TextEditorDocumentPreconditionsService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TextEditorDocumentPreconditionsServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void checksLoadRequiredPointers();
    void checksSaveRequiredPointersAndMissingPathMessage();
};

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

void TextEditorDocumentPreconditionsServiceTest::checksLoadRequiredPointers()
{
    const auto valid = validLoadPreconditions();
    QVERIFY(TextEditorDocumentPreconditionsService::canLoad(valid));

    auto missingEditor = valid;
    missingEditor.hasEditor = false;
    QVERIFY(!TextEditorDocumentPreconditionsService::canLoad(missingEditor));

    auto missingBlockSelectionKind = valid;
    missingBlockSelectionKind.hasBlockDetailsSelectedKind = false;
    QVERIFY(!TextEditorDocumentPreconditionsService::canLoad(missingBlockSelectionKind));
}

void TextEditorDocumentPreconditionsServiceTest::checksSaveRequiredPointersAndMissingPathMessage()
{
    const auto valid = validSavePreconditions();
    QString errorMessage;
    QVERIFY(TextEditorDocumentPreconditionsService::canSave(valid,
                                                            QStringLiteral("missing path"),
                                                            &errorMessage));

    auto missingCleanSnapshot = valid;
    missingCleanSnapshot.hasCleanTextSnapshot = false;
    errorMessage.clear();
    QVERIFY(!TextEditorDocumentPreconditionsService::canSave(missingCleanSnapshot,
                                                             QStringLiteral("missing path"),
                                                             &errorMessage));
    QVERIFY(errorMessage.isEmpty());

    auto missingPath = valid;
    missingPath.filePath.clear();
    errorMessage.clear();
    QVERIFY(!TextEditorDocumentPreconditionsService::canSave(missingPath,
                                                             QStringLiteral("missing path"),
                                                             &errorMessage));
    QCOMPARE(errorMessage, QStringLiteral("missing path"));
}
}

int runTextEditorDocumentPreconditionsServiceTest(int argc, char **argv)
{
    TextEditorDocumentPreconditionsServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TextEditorDocumentPreconditionsServiceTest.moc"
