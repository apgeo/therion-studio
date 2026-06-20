#include "../src/app/text_editor/map_editor/MapEditorCanvasEditController.h"
#include "../src/app/text_editor/map_editor/MapEditorObjectDetailsEditController.h"
#include "../src/app/text_editor/TextEditorTab.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/QtFileSystem.h"
#include "../src/core/TherionDocumentParser.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QUndoStack>

#include <iostream>
#include <optional>

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
    const QString filePath = tempDir.path() + QStringLiteral("/object-details-quick-fields.th2");
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

void setComboText(QComboBox *combo, const QString &text)
{
    combo->setEditable(true);
    combo->clear();
    combo->addItem(text);
    combo->setCurrentText(text);
}

MapEditorCanvasEditController makeCanvasController(TextEditorTab *tab,
                                                    QUndoStack *undoStack,
                                                    QString *toolbarStatus,
                                                    bool *commandApplyInProgress,
                                                    int *refreshCount,
                                                    int *flushCount)
{
    MapEditorCanvasEditContext context;
    context.textEditor = tab;
    context.undoStack = undoStack;
    context.toolbarStatusNote = toolbarStatus;
    context.commandApplyInProgress = commandApplyInProgress;
    context.refreshToolbarSummary = [refreshCount]() {
        ++(*refreshCount);
    };
    context.flushPendingSceneRefreshAfterCommand = [flushCount]() {
        ++(*flushCount);
    };
    return MapEditorCanvasEditController(context);
}

std::optional<InspectorObjectQuickFields> firstLineQuickFields(const QString &text)
{
    const QString firstLine = text.section(QLatin1Char('\n'), 0, 0);
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(firstLine, 1);
    return inspectorObjectQuickFieldsFromParsedLine(parsedLine);
}

bool expectRoundTrippedQuickFields(const QString &text,
                                  const QString &type,
                                  const QString &identifier,
                                  const QString &labelText,
                                  const QString &value,
                                  const char *message)
{
    const std::optional<InspectorObjectQuickFields> fields = firstLineQuickFields(text);
    if (!fields.has_value()) {
        std::cerr << message << '\n';
        std::cerr << "  quick fields were not extracted from the rewritten source line.\n";
        return false;
    }
    if (fields->type == type
        && fields->identifier == identifier
        && fields->text == labelText
        && fields->value == value) {
        return true;
    }

    std::cerr << message << '\n';
    std::cerr << "  expected type/id/text/value: "
              << type.toStdString() << " / "
              << identifier.toStdString() << " / "
              << labelText.toStdString() << " / "
              << value.toStdString() << '\n';
    std::cerr << "  actual type/id/text/value: "
              << fields->type.toStdString() << " / "
              << fields->identifier.toStdString() << " / "
              << fields->text.toStdString() << " / "
              << fields->value.toStdString() << '\n';
    return false;
}

struct QuickFieldsFixture
{
    bool updatingUi = false;
    int selectedObjectLineNumber = 1;
    int selectedObjectVertexIndex = 0;
    QString selectedObjectKind = QStringLiteral("point");
    QString objectQuickCommandKind = QStringLiteral("point");
    QString toolbarStatus;
    int refreshCount = 0;
    int snapshotCount = 0;
    QString snapshotLabel;
    QString snapshotBeforeText;
    QString snapshotAfterText;
    int snapshotLineNumber = 0;
    InspectorSymbolCatalog inspectorCatalog;
    MapEditorOrientationApplicabilityByCommand orientationApplicability;
    QComboBox typeCombo;
    QComboBox subtypeCombo;
    QComboBox projectionCombo;
    QLineEdit identifierEdit;
    QLineEdit nameEdit;
    QLineEdit textEdit;
    QLineEdit valueEdit;
    QCheckBox orientationEnabledCheck;
    QDoubleSpinBox orientationSpin;
    QCheckBox linePointLeftSizeEnabledCheck;
    QDoubleSpinBox linePointLeftSizeSpin;
    QComboBox linePointSegmentSubtypeCombo;
    QCheckBox linePointAltitudeAutoCheck;
    QPlainTextEdit linePointFlagsEdit;
};

MapEditorObjectDetailsContext makeDetailsContext(QuickFieldsFixture *fixture,
                                                  TextEditorTab *tab,
                                                  MapEditorCanvasEditController *canvasController)
{
    MapEditorObjectDetailsContext context;
    context.textEditor = tab;
    context.inspectorSymbolCatalog = &fixture->inspectorCatalog;
    context.orientationApplicabilityByCommand = &fixture->orientationApplicability;
    context.updatingUi = &fixture->updatingUi;
    context.selectedObjectLineNumber = &fixture->selectedObjectLineNumber;
    context.selectedObjectVertexIndex = &fixture->selectedObjectVertexIndex;
    context.selectedObjectKind = &fixture->selectedObjectKind;
    context.objectQuickCommandKind = &fixture->objectQuickCommandKind;
    context.toolbarStatusNote = &fixture->toolbarStatus;
    context.quickTypeCombo = &fixture->typeCombo;
    context.quickSubtypeCombo = &fixture->subtypeCombo;
    context.quickProjectionCombo = &fixture->projectionCombo;
    context.quickIdentifierEdit = &fixture->identifierEdit;
    context.quickNameEdit = &fixture->nameEdit;
    context.quickTextEdit = &fixture->textEdit;
    context.quickValueEdit = &fixture->valueEdit;
    context.orientationEnabledCheck = &fixture->orientationEnabledCheck;
    context.orientationSpin = &fixture->orientationSpin;
    context.linePointLeftSizeEnabledCheck = &fixture->linePointLeftSizeEnabledCheck;
    context.linePointLeftSizeSpin = &fixture->linePointLeftSizeSpin;
    context.linePointSegmentSubtypeCombo = &fixture->linePointSegmentSubtypeCombo;
    context.linePointAltitudeAutoCheck = &fixture->linePointAltitudeAutoCheck;
    context.linePointFlagsEdit = &fixture->linePointFlagsEdit;
    context.refreshToolbarSummary = [fixture]() {
        ++fixture->refreshCount;
    };
    context.refreshObjectDetailsPanel = []() {};
    context.applySourceTextChangeWithSnapshot = [fixture, canvasController](const QString &label,
                                                                            const QString &beforeText,
                                                                            const QString &afterText,
                                                                            int insertedLineNumber,
                                                                            std::function<void()> selectionRestoreHook) {
        ++fixture->snapshotCount;
        fixture->snapshotLabel = label;
        fixture->snapshotBeforeText = beforeText;
        fixture->snapshotAfterText = afterText;
        fixture->snapshotLineNumber = insertedLineNumber;
        if (selectionRestoreHook) {
            return canvasController->applySourceTextChangeWithSnapshot(label,
                                                                       beforeText,
                                                                       afterText,
                                                                       insertedLineNumber,
                                                                       TextEditorSourceSelectionRestorePolicy::CustomHook,
                                                                       std::move(selectionRestoreHook));
        }

        return canvasController->applySourceTextChangeWithSnapshot(label, beforeText, afterText, insertedLineNumber);
    };
    return context;
}

int runLabelQuickFieldsTextTransactionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("point 1 2 label -id old -text old # keep\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create label quick-fields test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load label quick-fields test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString canvasToolbarStatus;
    bool commandApplyInProgress = false;
    int canvasRefreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController canvasController = makeCanvasController(&tab,
                                                                          &undoStack,
                                                                          &canvasToolbarStatus,
                                                                          &commandApplyInProgress,
                                                                          &canvasRefreshCount,
                                                                          &flushCount);

    QuickFieldsFixture fixture;
    setComboText(&fixture.typeCombo, QStringLiteral("label"));
    setComboText(&fixture.subtypeCombo, QString());
    fixture.identifierEdit.setText(QStringLiteral("label-1"));
    fixture.nameEdit.hide();
    fixture.textEdit.setText(QStringLiteral("Main passage"));
    fixture.textEdit.show();
    fixture.valueEdit.hide();

    MapEditorObjectDetailsEditController controller =
        MapEditorObjectDetailsEditController(makeDetailsContext(&fixture, &tab, &canvasController));
    controller.applyObjectQuickFieldEdits();
    pumpEvents();

    const QString expectedAfterText = QStringLiteral("point 1 2 label -id label-1 -text \"Main passage\" # keep\n");
    if (!expect(fixture.snapshotCount == 1, "Object quick-fields edit should use one source snapshot transaction.")) {
        return 1;
    }
    if (!expect(fixture.snapshotLabel == QStringLiteral("Edit Object Fields"),
                "Object quick-fields snapshot should use the object-fields undo label.")) {
        return 1;
    }
    if (!expect(fixture.snapshotBeforeText == beforeText && fixture.snapshotAfterText == expectedAfterText,
                "Object quick-fields snapshot should capture the full before/after source text.")) {
        return 1;
    }
    if (!expect(fixture.snapshotLineNumber == 1, "Object quick-fields snapshot should preserve the selected line number.")) {
        return 1;
    }
    if (!expect(tab.text() == expectedAfterText, "Object quick-fields transaction should apply label text edits.")) {
        return 1;
    }
    if (!expectRoundTrippedQuickFields(tab.text(),
                                      QStringLiteral("label"),
                                      QStringLiteral("label-1"),
                                      QStringLiteral("Main passage"),
                                      QString(),
                                      "Label quick-fields transaction should round-trip through inspector parsing.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Object quick-fields transaction should push one undo command.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore the label quick-fields source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == expectedAfterText, "Redo should restore the label quick-fields source text.")) {
        return 1;
    }
    if (!expectRoundTrippedQuickFields(tab.text(),
                                      QStringLiteral("label"),
                                      QStringLiteral("label-1"),
                                      QStringLiteral("Main passage"),
                                      QString(),
                                      "Redo label quick-fields source text should round-trip through inspector parsing.")) {
        return 1;
    }

    return 0;
}

int runPointQuickFieldsValueTransactionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("point 1 2 passage-height -value [+4 -2 m] # keep\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create point value quick-fields test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load point value quick-fields test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString canvasToolbarStatus;
    bool commandApplyInProgress = false;
    int canvasRefreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController canvasController = makeCanvasController(&tab,
                                                                          &undoStack,
                                                                          &canvasToolbarStatus,
                                                                          &commandApplyInProgress,
                                                                          &canvasRefreshCount,
                                                                          &flushCount);

    QuickFieldsFixture fixture;
    fixture.inspectorCatalog.valueOptionTypesByCommand.insert(QStringLiteral("point"), QSet<QString>{QStringLiteral("passage-height")});
    setComboText(&fixture.typeCombo, QStringLiteral("passage-height"));
    setComboText(&fixture.subtypeCombo, QString());
    fixture.identifierEdit.setText(QStringLiteral("height-1"));
    fixture.nameEdit.hide();
    fixture.textEdit.hide();
    fixture.valueEdit.setText(QStringLiteral("3.5"));
    fixture.valueEdit.show();

    MapEditorObjectDetailsEditController controller =
        MapEditorObjectDetailsEditController(makeDetailsContext(&fixture, &tab, &canvasController));
    controller.applyObjectQuickFieldEdits();
    pumpEvents();

    const QString expectedAfterText = QStringLiteral("point 1 2 passage-height -value 3.5 -id height-1 # keep\n");
    if (!expect(fixture.snapshotCount == 1, "Point value quick-fields edit should use one source snapshot transaction.")) {
        return 1;
    }
    if (!expect(fixture.snapshotBeforeText == beforeText && fixture.snapshotAfterText == expectedAfterText,
                "Point value quick-fields snapshot should capture the composed source edit.")) {
        return 1;
    }
    if (!expect(tab.text() == expectedAfterText, "Point value quick-fields transaction should apply value edits.")) {
        return 1;
    }
    if (!expectRoundTrippedQuickFields(tab.text(),
                                      QStringLiteral("passage-height"),
                                      QStringLiteral("height-1"),
                                      QString(),
                                      QStringLiteral("3.5"),
                                      "Point value quick-fields transaction should round-trip through inspector parsing.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Point value quick-fields transaction should push one undo command.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore the point value quick-fields source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == expectedAfterText, "Redo should restore the point value quick-fields source text.")) {
        return 1;
    }
    if (!expectRoundTrippedQuickFields(tab.text(),
                                      QStringLiteral("passage-height"),
                                      QStringLiteral("height-1"),
                                      QString(),
                                      QStringLiteral("3.5"),
                                      "Redo point value quick-fields source text should round-trip through inspector parsing.")) {
        return 1;
    }

    return 0;
}

int runLineClosedToggleTransactionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("line wall # keep\n  0 0\n  1 1\nendline\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create line closed toggle test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load line closed toggle test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString canvasToolbarStatus;
    bool commandApplyInProgress = false;
    int canvasRefreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController canvasController = makeCanvasController(&tab,
                                                                          &undoStack,
                                                                          &canvasToolbarStatus,
                                                                          &commandApplyInProgress,
                                                                          &canvasRefreshCount,
                                                                          &flushCount);

    QuickFieldsFixture fixture;
    fixture.selectedObjectKind = QStringLiteral("line");
    MapEditorObjectDetailsEditController controller =
        MapEditorObjectDetailsEditController(makeDetailsContext(&fixture, &tab, &canvasController));
    controller.handleLineClosedToggled(true);
    pumpEvents();

    const QString expectedAfterText = QStringLiteral("line wall -close on # keep\n  0 0\n  1 1\nendline\n");
    if (!expect(fixture.snapshotCount == 1, "Line closed toggle should use one source snapshot transaction.")) {
        return 1;
    }
    if (!expect(fixture.snapshotLabel == QStringLiteral("Edit Line Closed"),
                "Line closed toggle snapshot should use the line-closed undo label.")) {
        return 1;
    }
    if (!expect(fixture.snapshotBeforeText == beforeText && fixture.snapshotAfterText == expectedAfterText,
                "Line closed toggle snapshot should capture the full before/after source text.")) {
        return 1;
    }
    if (!expect(tab.text() == expectedAfterText, "Line closed toggle transaction should apply source text edits.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Line closed toggle transaction should push one undo command.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore the line closed toggle source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == expectedAfterText, "Redo should restore the line closed toggle source text.")) {
        return 1;
    }

    return 0;
}

int runPointOrientationTransactionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("point 10 20 station -name a1 # keep\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create point orientation test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load point orientation test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString canvasToolbarStatus;
    bool commandApplyInProgress = false;
    int canvasRefreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController canvasController = makeCanvasController(&tab,
                                                                          &undoStack,
                                                                          &canvasToolbarStatus,
                                                                          &commandApplyInProgress,
                                                                          &canvasRefreshCount,
                                                                          &flushCount);

    QuickFieldsFixture fixture;
    fixture.selectedObjectKind = QStringLiteral("point");
    fixture.orientationApplicability.insert(QStringLiteral("point"), MapEditorOrientationApplicability{});
    fixture.orientationEnabledCheck.setChecked(true);
    fixture.orientationSpin.setRange(-720.0, 720.0);
    fixture.orientationSpin.setValue(370.0);
    fixture.linePointLeftSizeEnabledCheck.hide();

    MapEditorObjectDetailsEditController controller =
        MapEditorObjectDetailsEditController(makeDetailsContext(&fixture, &tab, &canvasController));
    controller.applyObjectOrientationEdits();
    pumpEvents();

    const QString expectedAfterText = QStringLiteral("point 10 20 station -name a1 -orientation 10 # keep\n");
    if (!expect(fixture.snapshotCount == 1, "Point orientation edit should use one source snapshot transaction.")) {
        return 1;
    }
    if (!expect(fixture.snapshotLabel == QStringLiteral("Edit Object Orientation"),
                "Point orientation snapshot should use the object-orientation undo label.")) {
        return 1;
    }
    if (!expect(fixture.snapshotBeforeText == beforeText && fixture.snapshotAfterText == expectedAfterText,
                "Point orientation snapshot should capture the full before/after source text.")) {
        return 1;
    }
    if (!expect(tab.text() == expectedAfterText, "Point orientation transaction should apply source text edits.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Point orientation transaction should push one undo command.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore the point orientation source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == expectedAfterText, "Redo should restore the point orientation source text.")) {
        return 1;
    }

    return 0;
}

int runLineAnchorOrientationTransactionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("line slope\n  10 20 -orientation 45\n  30 40\nendline\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create line anchor orientation test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load line anchor orientation test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString canvasToolbarStatus;
    bool commandApplyInProgress = false;
    int canvasRefreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController canvasController = makeCanvasController(&tab,
                                                                          &undoStack,
                                                                          &canvasToolbarStatus,
                                                                          &commandApplyInProgress,
                                                                          &canvasRefreshCount,
                                                                          &flushCount);

    QuickFieldsFixture fixture;
    fixture.selectedObjectKind = QStringLiteral("line");
    fixture.selectedObjectVertexIndex = 0;
    fixture.orientationApplicability.insert(QStringLiteral("line"), MapEditorOrientationApplicability{});
    fixture.orientationEnabledCheck.setChecked(true);
    fixture.orientationSpin.setRange(-720.0, 720.0);
    fixture.orientationSpin.setValue(90.0);
    fixture.linePointLeftSizeEnabledCheck.hide();

    MapEditorObjectDetailsEditController controller =
        MapEditorObjectDetailsEditController(makeDetailsContext(&fixture, &tab, &canvasController));
    controller.applyObjectOrientationEdits();
    pumpEvents();

    const QString expectedAfterText = QStringLiteral("line slope\n  10 20 -orientation 90\n  30 40\nendline\n");
    if (!expect(fixture.snapshotCount == 1, "Line anchor orientation edit should use one source snapshot transaction.")) {
        return 1;
    }
    if (!expect(fixture.snapshotLabel == QStringLiteral("Edit Object Orientation"),
                "Line anchor orientation snapshot should use the object-orientation undo label.")) {
        return 1;
    }
    if (!expect(fixture.snapshotBeforeText == beforeText && fixture.snapshotAfterText == expectedAfterText,
                "Line anchor orientation snapshot should capture the full before/after source text.")) {
        return 1;
    }
    if (!expect(tab.text() == expectedAfterText, "Line anchor orientation transaction should apply source text edits.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Line anchor orientation transaction should push one undo command.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore the line anchor orientation source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == expectedAfterText, "Redo should restore the line anchor orientation source text.")) {
        return 1;
    }

    return 0;
}

int runScrapProjectionTransactionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("scrap s1 # keep\nendscrap\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create scrap projection test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load scrap projection test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString canvasToolbarStatus;
    bool commandApplyInProgress = false;
    int canvasRefreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController canvasController = makeCanvasController(&tab,
                                                                          &undoStack,
                                                                          &canvasToolbarStatus,
                                                                          &commandApplyInProgress,
                                                                          &canvasRefreshCount,
                                                                          &flushCount);

    QuickFieldsFixture fixture;
    setComboText(&fixture.projectionCombo, QStringLiteral("plan"));

    MapEditorObjectDetailsEditController controller =
        MapEditorObjectDetailsEditController(makeDetailsContext(&fixture, &tab, &canvasController));
    controller.applyScrapProjectionEdit();
    pumpEvents();

    const QString expectedAfterText = QStringLiteral("scrap s1 -projection plan # keep\nendscrap\n");
    if (!expect(fixture.snapshotCount == 1, "Scrap projection edit should use one source snapshot transaction.")) {
        return 1;
    }
    if (!expect(fixture.snapshotLabel == QStringLiteral("Edit Scrap Projection"),
                "Scrap projection snapshot should use the scrap-projection undo label.")) {
        return 1;
    }
    if (!expect(fixture.snapshotBeforeText == beforeText && fixture.snapshotAfterText == expectedAfterText,
                "Scrap projection snapshot should capture the full before/after source text.")) {
        return 1;
    }
    if (!expect(tab.text() == expectedAfterText, "Scrap projection transaction should apply source text edits.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 1, "Scrap projection transaction should push one undo command.")) {
        return 1;
    }

    undoStack.undo();
    pumpEvents();
    if (!expect(tab.text() == beforeText, "Undo should restore the scrap projection source text.")) {
        return 1;
    }

    undoStack.redo();
    pumpEvents();
    if (!expect(tab.text() == expectedAfterText, "Redo should restore the scrap projection source text.")) {
        return 1;
    }

    return 0;
}

int runLinePointOptionsStaleTransactionDoesNotReportSuccessTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Failed to create temporary directory.")) {
        return 1;
    }

    const QString beforeText = QStringLiteral("line wall\n  0 0\n  1 1\nendline\n");
    const QString filePath = createTestFile(tempDir, beforeText.toUtf8());
    if (!expect(!filePath.isEmpty(), "Failed to create line-point stale transaction test file.")) {
        return 1;
    }

    QtFileSystem fileSystem;
    TextEditorTab tab{fileSystem, CommandCatalogStore()};
    if (!expect(loadTestTab(&tab, filePath), "Failed to load line-point stale transaction test tab.")) {
        return 1;
    }

    QUndoStack undoStack;
    QString canvasToolbarStatus;
    bool commandApplyInProgress = false;
    int canvasRefreshCount = 0;
    int flushCount = 0;
    MapEditorCanvasEditController canvasController = makeCanvasController(&tab,
                                                                          &undoStack,
                                                                          &canvasToolbarStatus,
                                                                          &commandApplyInProgress,
                                                                          &canvasRefreshCount,
                                                                          &flushCount);

    QuickFieldsFixture fixture;
    fixture.selectedObjectKind = QStringLiteral("line");
    fixture.selectedObjectLineNumber = 1;
    fixture.selectedObjectVertexIndex = 0;
    MapEditorObjectDetailsContext context = makeDetailsContext(&fixture, &tab, &canvasController);
    context.applySourceTextChangeWithSnapshot = [&fixture](const QString &label,
                                                           const QString &beforeText,
                                                           const QString &afterText,
                                                           int insertedLineNumber,
                                                           std::function<void()> selectionRestoreHook) {
        Q_UNUSED(selectionRestoreHook);
        ++fixture.snapshotCount;
        fixture.snapshotLabel = label;
        fixture.snapshotBeforeText = beforeText;
        fixture.snapshotAfterText = afterText;
        fixture.snapshotLineNumber = insertedLineNumber;
        fixture.toolbarStatus = QStringLiteral("Source transaction skipped: document changed.");
        return TextEditorSourceTransactionResult::Stale;
    };

    MapEditorObjectDetailsEditController controller = MapEditorObjectDetailsEditController(context);
    fixture.linePointFlagsEdit.setPlainText(QStringLiteral("altitude ."));
    controller.applyLinePointFlagsEdits();
    if (!expect(fixture.snapshotCount == 1,
                "Stale line-point options edit should still attempt one source transaction.")) {
        return 1;
    }
    if (!expect(fixture.snapshotLabel == QStringLiteral("Edit Line Point Options"),
                "Stale line-point options edit should use the line-point options undo label.")) {
        return 1;
    }
    if (!expect(tab.text() == beforeText,
                "Stale line-point options transaction should not rewrite source text.")) {
        return 1;
    }
    if (!expect(undoStack.count() == 0,
                "Stale line-point options transaction should not push an undo command.")) {
        return 1;
    }
    if (!expect(fixture.toolbarStatus == QStringLiteral("Source transaction skipped: document changed."),
                "Stale line-point options transaction should not be overwritten by a success status.")) {
        return 1;
    }
    if (!expect(!fixture.toolbarStatus.contains(QStringLiteral("Updated additional line-point options")),
                "Stale line-point options transaction must not report a successful update.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    if (const int result = runLabelQuickFieldsTextTransactionTest(); result != 0) {
        return result;
    }
    if (const int result = runPointQuickFieldsValueTransactionTest(); result != 0) {
        return result;
    }
    if (const int result = runLineClosedToggleTransactionTest(); result != 0) {
        return result;
    }
    if (const int result = runPointOrientationTransactionTest(); result != 0) {
        return result;
    }
    if (const int result = runLineAnchorOrientationTransactionTest(); result != 0) {
        return result;
    }
    if (const int result = runScrapProjectionTransactionTest(); result != 0) {
        return result;
    }
    return runLinePointOptionsStaleTransactionDoesNotReportSuccessTest();
}
