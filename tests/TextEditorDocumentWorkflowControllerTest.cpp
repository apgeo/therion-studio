#include "../src/app/text_editor/TextEditorDocumentWorkflowController.h"

#include <QCoreApplication>
#include <QStringList>

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

int runPostLoadWorkflowOrderWithDisableTest()
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

    if (!expect(calls == expected,
                "Post-load workflow order with disableBlocksMode changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runPostLoadWorkflowSkipsDisableWhenNotRequestedTest()
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

    if (!expect(calls == expected,
                "Post-load workflow should skip blocks-mode disable when not requested.")) {
        return 1;
    }

    return 0;
}

int runPostSaveWorkflowOrderTest()
{
    QStringList calls;

    TextEditorDocumentWorkflowController::SaveActions actions;
    actions.refreshTitle = [&calls]() { calls.append(QStringLiteral("refresh_title")); };
    actions.dirtyStateChanged = [&calls](bool dirty) { calls.append(QStringLiteral("dirty_changed:%1").arg(dirty ? QStringLiteral("true") : QStringLiteral("false"))); };

    TextEditorDocumentWorkflowController::runPostSaveWorkflow(actions);

    const QStringList expected = {
        QStringLiteral("refresh_title"),
        QStringLiteral("dirty_changed:false")};

    if (!expect(calls == expected,
                "Post-save workflow order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runProjectRootWorkflowTest()
{
    QStringList calls;

    TextEditorDocumentWorkflowController::ProjectRootActions actions;
    actions.refreshStatus = [&calls]() { calls.append(QStringLiteral("refresh_status")); };

    TextEditorDocumentWorkflowController::runPostProjectRootSetWorkflow(actions);

    if (!expect(calls == QStringList{QStringLiteral("refresh_status")},
                "Project-root workflow should only trigger refreshStatus.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runPostLoadWorkflowOrderWithDisableTest() != 0) {
        return 1;
    }
    if (runPostLoadWorkflowSkipsDisableWhenNotRequestedTest() != 0) {
        return 1;
    }
    if (runPostSaveWorkflowOrderTest() != 0) {
        return 1;
    }
    return runProjectRootWorkflowTest();
}
