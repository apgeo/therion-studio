#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>

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

void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QString createTestFile(QTemporaryDir &tempDir, const QByteArray &contents)
{
    const QString filePath = tempDir.path() + QStringLiteral("/import-insertion-transaction.th");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(contents);
    file.close();
    return filePath;
}

bool loadTestTab(TextEditorTab *tab, const QString &filePath)
{
    QString errorMessage;
    if (!tab->loadFile(filePath, &errorMessage)) {
        std::cerr << errorMessage.toStdString() << '\n';
        return false;
    }
    tab->resize(640, 480);
    tab->show();
    pumpEvents();
    return true;
}

int runImportInsertionUndoRedoTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("survey demo\nendsurvey\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create import-insertion test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load import-insertion test tab.")) {
        return 1;
    }

    const QString insertedText = QStringLiteral("centerline\nendcenterline\n");
    const bool applied = tab.insertTextAtImportInsertionPoint(insertedText);
    pumpEvents();
    if (!expect(applied,
                "Import insertion transaction should apply non-empty source text.")) {
        return 1;
    }

    const QString afterText = tab.text();
    if (!expect(afterText != beforeText && afterText.contains(insertedText),
                "Import insertion transaction should update the source text and preserve inserted payload.")) {
        return 1;
    }

    if (!expect(tab.canUndo(), "Import insertion transaction should produce an undoable change.")) {
        return 1;
    }

    tab.triggerUndo();
    pumpEvents();
    if (!expect(tab.text() == beforeText,
                "Import insertion undo should restore the original source text.")) {
        return 1;
    }

    tab.triggerRedo();
    pumpEvents();
    if (!expect(tab.text() == afterText,
                "Import insertion redo should reapply the inserted source text.")) {
        return 1;
    }

    return 0;
}

int runEmptyInsertionRejectedTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("survey demo\nendsurvey\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create empty-insertion test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load empty-insertion test tab.")) {
        return 1;
    }

    const bool applied = tab.insertTextAtImportInsertionPoint(QString());
    pumpEvents();
    if (!expect(!applied,
                "Import insertion transaction should reject empty payloads.")) {
        return 1;
    }

    if (!expect(tab.text() == beforeText,
                "Rejected import insertion should preserve the original source text.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (runImportInsertionUndoRedoTest() != 0) {
        return 1;
    }
    if (runEmptyInsertionRejectedTest() != 0) {
        return 1;
    }

    return 0;
}
