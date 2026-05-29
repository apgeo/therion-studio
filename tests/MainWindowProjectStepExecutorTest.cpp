#include "../src/app/MainWindowProjectOrchestrationService.h"
#include "../src/app/MainWindowProjectStepExecutor.h"

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

int runOpenProjectExecutionOrderTest()
{
    QStringList calls;

    MainWindowProjectStepExecutor::OpenProjectActions actions;
    actions.applyProjectRootToBrowser = [&calls]() { calls.append(QStringLiteral("apply_root")); };
    actions.loadStructureNameOverrides = [&calls]() { calls.append(QStringLiteral("load_overrides")); };
    actions.syncOpenDocumentsToProjectRoot = [&calls]() { calls.append(QStringLiteral("sync_docs")); };
    actions.persistSessionLastProjectPath = [&calls]() { calls.append(QStringLiteral("persist_last")); };
    actions.rebuildStructureSidebar = [&calls]() { calls.append(QStringLiteral("rebuild_structure")); };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };
    actions.ensureWelcomeTab = [&calls]() { calls.append(QStringLiteral("ensure_welcome")); };

    MainWindowProjectStepExecutor::executeOpenProjectSteps(
        MainWindowProjectOrchestrationService::buildOpenProjectSteps(true), actions);

    const QStringList expected = {
        QStringLiteral("apply_root"),
        QStringLiteral("load_overrides"),
        QStringLiteral("sync_docs"),
        QStringLiteral("persist_last"),
        QStringLiteral("rebuild_structure"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions"),
        QStringLiteral("ensure_welcome")};
    if (!expect(calls == expected,
                "Open-project step executor order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectExecutionOrderTest()
{
    QStringList calls;

    MainWindowProjectStepExecutor::CloseProjectActions actions;
    actions.clearDocumentTabs = [&calls]() { calls.append(QStringLiteral("clear_tabs")); };
    actions.resetProjectBrowser = [&calls]() { calls.append(QStringLiteral("reset_browser")); };
    actions.persistSessionLastProjectPath = [&calls]() { calls.append(QStringLiteral("persist_last")); };
    actions.persistOpenDocuments = [&calls]() { calls.append(QStringLiteral("persist_docs")); };
    actions.rebuildStructureSidebar = [&calls]() { calls.append(QStringLiteral("rebuild_structure")); };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };

    MainWindowProjectStepExecutor::executeCloseProjectSteps(
        MainWindowProjectOrchestrationService::buildCloseProjectSteps(), actions);

    const QStringList expected = {
        QStringLiteral("clear_tabs"),
        QStringLiteral("reset_browser"),
        QStringLiteral("persist_last"),
        QStringLiteral("persist_docs"),
        QStringLiteral("rebuild_structure"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions")};
    if (!expect(calls == expected,
                "Close-project step executor order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runOpenProjectExecutionOrderTest() != 0) {
        return 1;
    }
    if (runCloseProjectExecutionOrderTest() != 0) {
        return 1;
    }

    return 0;
}
