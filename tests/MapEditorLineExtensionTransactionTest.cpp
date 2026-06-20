#include "../src/app/text_editor/map_editor/MapEditorTab.h"
#include "../src/app/text_editor/map_editor/MapEditorSourceReferenceResolver.h"

#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "FakeSessionStore.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QMainWindow>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>

using namespace TherionStudio;

namespace
{
void pumpEvents()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QString createTestFile(QTemporaryDir &tempDir, const QByteArray &contents)
{
    const QString filePath = tempDir.path() + QStringLiteral("/line-extension-transaction.th2");
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return {};
    }
    file.write(contents);
    file.close();
    return filePath;
}
}

class MapEditorLineExtensionTransactionTest final : public QObject
{
    Q_OBJECT

private slots:
    void extendAfterCommitsAndExitsDraftMode();
};

void MapEditorLineExtensionTransactionTest::extendAfterCommitsAndExitsDraftMode()
{
    QTemporaryDir tempDir;
    QVERIFY2(tempDir.isValid(), "Failed to create temporary directory.");

    const QString filePath = createTestFile(tempDir,
                                            "encoding utf-8\n"
                                            "scrap s1 -projection plan\n"
                                            "line wall\n"
                                            "  0 0\n"
                                            "  10 0\n"
                                            "endline\n"
                                            "endscrap\n");
    QVERIFY2(!filePath.isEmpty(), "Failed to create line extension transaction test file.");

    QtFileSystem fileSystem;
    FakeSessionStore sessionStore;
    QMainWindow hostWindow;
    hostWindow.resize(800, 600);
    auto *central = new QWidget(&hostWindow);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *mapTab = new MapEditorTab(fileSystem, sessionStore, CommandCatalogStore(), central);
    layout->addWidget(mapTab);
    hostWindow.setCentralWidget(central);
    hostWindow.show();
    pumpEvents();

    QString errorMessage;
    const bool loaded = mapTab->loadFile(filePath, &errorMessage);
    const QByteArray loadFailureMessage =
        errorMessage.isEmpty()
            ? QByteArray("Failed to load line extension transaction fixture.")
            : QByteArray("Failed to load line extension transaction fixture: ") + errorMessage.toUtf8();
    QVERIFY2(loaded, loadFailureMessage.constData());
    pumpEvents();

    constexpr int lineNumber = 3;
    const std::optional<MapGeometryFeature> lineFeature = lineFeatureForLineNumber(mapTab->text(), lineNumber);
    QVERIFY2(lineFeature.has_value() && lineFeature->lineVertices.size() == 2,
             "Line extension transaction fixture should expose a two-vertex wall line.");
    const int lastSourceVertexIndex = lineFeature->lineVertices.last().anchorSourceVertexIndex;
    QVERIFY2(lastSourceVertexIndex >= 0,
             "Line extension transaction fixture should resolve the terminal source vertex index.");

    QVERIFY2(mapTab->testBeginLineExtensionFromSelection(lineNumber, lastSourceVertexIndex, false),
             "Extend After should begin from the terminal selected line vertex.");
    QVERIFY2(mapTab->testLineExtensionActive(),
             "Beginning Extend After should mark line extension mode active.");
    QCOMPARE(mapTab->testInteractiveDrawMode(), MapEditorTab::InteractiveDrawMode::Line);
    QCOMPARE(mapTab->testInteractiveDrawLineVertexCount(), 1);

    MapEditorInteractiveLineDraftVertex appendedVertex;
    appendedVertex.anchorSource = QPointF(20.0, 0.0);
    appendedVertex.anchorScene = mapTab->testScenePointFromSourcePosition(appendedVertex.anchorSource);
    mapTab->testAppendInteractiveDrawLineVertex(appendedVertex);

    QVERIFY2(mapTab->testCommitLineExtensionSession(),
             "Committing Extend After should report the extension workflow as handled.");
    pumpEvents();

    QVERIFY2(mapTab->text().contains(QStringLiteral("20.0 0.0")),
             qPrintable(QStringLiteral("Committing Extend After should append the new terminal coordinate row to source text.\n%1")
                            .arg(mapTab->text())));
    QVERIFY2(!mapTab->testLineExtensionActive(),
             "Successful Extend After commit should exit draft extension mode.");
    QCOMPARE(mapTab->testInteractiveDrawMode(), MapEditorTab::InteractiveDrawMode::None);
    QCOMPARE(mapTab->testInteractiveDrawLineVertexCount(), 0);
    QVERIFY2(mapTab->canUndo(), "Successful Extend After commit should create an undoable source transaction.");

    const QString extendedText = mapTab->text();
    mapTab->triggerUndo();
    pumpEvents();
    QVERIFY2(!mapTab->text().contains(QStringLiteral("20.0 0.0")),
             "Undo after Extend After should remove the appended terminal coordinate row.");
    mapTab->triggerRedo();
    pumpEvents();
    QCOMPARE(mapTab->text(), extendedText);
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    MapEditorLineExtensionTransactionTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorLineExtensionTransactionTest.moc"
