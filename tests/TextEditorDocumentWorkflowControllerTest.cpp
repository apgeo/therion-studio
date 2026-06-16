#include "../src/app/text_editor/TextEditorDocumentWorkflowController.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TextEditorDocumentWorkflowControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void runsPostLoadWorkflowWithDisable();
    void skipsPostLoadDisableWhenNotRequested();
    void runsPostSaveWorkflow();
    void runsProjectRootWorkflow();
};

void TextEditorDocumentWorkflowControllerTest::runsPostLoadWorkflowWithDisable()
{
    QStringList calls;

    TextEditorDocumentWorkflowController::LoadActions actions;
    actions.refreshBlocksModeAvailability = [&calls]() { calls.append(QStringLiteral("refresh_blocks_mode")); };
    actions.setBlocksModeActive = [&calls](bool active) { calls.append(QStringLiteral("set_blocks_mode:%1").arg(active ? QStringLiteral("true") : QStringLiteral("false"))); };
    actions.rebuildBlocksCanvasFromText = [&calls]() { calls.append(QStringLiteral("rebuild_canvas")); };
    actions.clearBlockDetailsPane = [&calls]() { calls.append(QStringLiteral("clear_details")); };
    actions.populateBlockToolbox = [&calls]() { calls.append(QStringLiteral("populate_toolbox")); };
    actions.refreshEditorModeUi = [&calls]() { calls.append(QStringLiteral("refresh_editor_mode")); };
    actions.refreshTitle = [&calls]() { calls.append(QStringLiteral("refresh_title")); };
    actions.refreshCurrentLineHighlight = [&calls]() { calls.append(QStringLiteral("refresh_line_highlight")); };
    actions.dirtyStateChanged = [&calls](bool dirty) { calls.append(QStringLiteral("dirty_changed:%1").arg(dirty ? QStringLiteral("true") : QStringLiteral("false"))); };
    actions.updateContextHelp = [&calls]() { calls.append(QStringLiteral("update_context_help")); };

    TextEditorDocumentWorkflowController::runPostLoadWorkflow(true, actions);

    const QStringList expected = {
        QStringLiteral("refresh_blocks_mode"),
        QStringLiteral("set_blocks_mode:false"),
        QStringLiteral("rebuild_canvas"),
        QStringLiteral("clear_details"),
        QStringLiteral("populate_toolbox"),
        QStringLiteral("refresh_editor_mode"),
        QStringLiteral("refresh_title"),
        QStringLiteral("refresh_line_highlight"),
        QStringLiteral("dirty_changed:false"),
        QStringLiteral("update_context_help")};

    QCOMPARE(calls, expected);
}

void TextEditorDocumentWorkflowControllerTest::skipsPostLoadDisableWhenNotRequested()
{
    QStringList calls;

    TextEditorDocumentWorkflowController::LoadActions actions;
    actions.refreshBlocksModeAvailability = [&calls]() { calls.append(QStringLiteral("refresh_blocks_mode")); };
    actions.setBlocksModeActive = [&calls](bool active) { calls.append(QStringLiteral("set_blocks_mode:%1").arg(active ? QStringLiteral("true") : QStringLiteral("false"))); };
    actions.dirtyStateChanged = [&calls](bool dirty) { calls.append(QStringLiteral("dirty_changed:%1").arg(dirty ? QStringLiteral("true") : QStringLiteral("false"))); };

    TextEditorDocumentWorkflowController::runPostLoadWorkflow(false, actions);

    const QStringList expected = {
        QStringLiteral("refresh_blocks_mode"),
        QStringLiteral("dirty_changed:false")};

    QCOMPARE(calls, expected);
}

void TextEditorDocumentWorkflowControllerTest::runsPostSaveWorkflow()
{
    QStringList calls;

    TextEditorDocumentWorkflowController::SaveActions actions;
    actions.refreshTitle = [&calls]() { calls.append(QStringLiteral("refresh_title")); };
    actions.dirtyStateChanged = [&calls](bool dirty) { calls.append(QStringLiteral("dirty_changed:%1").arg(dirty ? QStringLiteral("true") : QStringLiteral("false"))); };

    TextEditorDocumentWorkflowController::runPostSaveWorkflow(actions);

    const QStringList expected = {
        QStringLiteral("refresh_title"),
        QStringLiteral("dirty_changed:false")};

    QCOMPARE(calls, expected);
}

void TextEditorDocumentWorkflowControllerTest::runsProjectRootWorkflow()
{
    QStringList calls;

    TextEditorDocumentWorkflowController::ProjectRootActions actions;
    actions.refreshStatus = [&calls]() { calls.append(QStringLiteral("refresh_status")); };

    TextEditorDocumentWorkflowController::runPostProjectRootSetWorkflow(actions);

    QCOMPARE(calls, QStringList{QStringLiteral("refresh_status")});
}
}

int runTextEditorDocumentWorkflowControllerTest(int argc, char **argv)
{
    TextEditorDocumentWorkflowControllerTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TextEditorDocumentWorkflowControllerTest.moc"
