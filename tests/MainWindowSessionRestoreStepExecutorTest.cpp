#include "../src/app/MainWindowSessionRestoreStepExecutor.h"

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

MainWindowSessionRestoreOrchestrationService::Plan makePlan(MainWindowSessionProjectService::ProjectRestoreStatus status,
                                                            const QString &path,
                                                            const std::vector<MainWindowSessionRestoreOrchestrationService::Step> &steps)
{
    MainWindowSessionRestoreOrchestrationService::Plan plan;
    plan.restoreProjectDecision.status = status;
    plan.restoreProjectDecision.projectPath = path;
    plan.steps = steps;
    return plan;
}

int runExecutionOrderAndPathForwardingTest()
{
    QStringList calls;

    MainWindowSessionRestoreStepExecutor::Actions actions;
    actions.applyProjectRootToBrowser = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("apply_root:%1").arg(projectPath));
    };
    actions.appendRestoredProjectRootConsole = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("restored_console:%1").arg(projectPath));
    };
    actions.loadStructureNameOverrides = [&calls]() { calls.append(QStringLiteral("load_overrides")); };
    actions.syncOpenDocumentsToProjectRoot = [&calls]() { calls.append(QStringLiteral("sync_docs")); };
    actions.rebuildStructureSidebar = [&calls]() { calls.append(QStringLiteral("rebuild_structure")); };
    actions.appendSkippedProtectedProjectConsole = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("skipped_console:%1").arg(projectPath));
    };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };
    actions.restoreOpenDocuments = [&calls]() { calls.append(QStringLiteral("restore_docs")); };

    MainWindowSessionRestoreStepExecutor::execute(
        makePlan(MainWindowSessionProjectService::ProjectRestoreStatus::Restored,
                 QStringLiteral("/tmp/project"),
                 {
                     MainWindowSessionRestoreOrchestrationService::Step::ApplyProjectRootToBrowser,
                     MainWindowSessionRestoreOrchestrationService::Step::AppendRestoredProjectRootConsole,
                     MainWindowSessionRestoreOrchestrationService::Step::LoadStructureNameOverrides,
                     MainWindowSessionRestoreOrchestrationService::Step::SyncOpenDocumentsToProjectRoot,
                     MainWindowSessionRestoreOrchestrationService::Step::RebuildStructureSidebar,
                     MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay,
                     MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState,
                     MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments}),
        actions);

    const QStringList expected = {
        QStringLiteral("apply_root:/tmp/project"),
        QStringLiteral("restored_console:/tmp/project"),
        QStringLiteral("load_overrides"),
        QStringLiteral("sync_docs"),
        QStringLiteral("rebuild_structure"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions"),
        QStringLiteral("restore_docs")};
    if (!expect(calls == expected,
                "Session-restore step execution order/path forwarding changed unexpectedly.")) {
        return 1;
    }

    return 0;
}

int runSkippedProtectedExecutionTest()
{
    QStringList calls;

    MainWindowSessionRestoreStepExecutor::Actions actions;
    actions.appendSkippedProtectedProjectConsole = [&calls](const QString &projectPath) {
        calls.append(QStringLiteral("skipped_console:%1").arg(projectPath));
    };
    actions.refreshTherionConfigDisplay = [&calls]() { calls.append(QStringLiteral("refresh_config")); };
    actions.updateProjectActionState = [&calls]() { calls.append(QStringLiteral("update_actions")); };
    actions.restoreOpenDocuments = [&calls]() { calls.append(QStringLiteral("restore_docs")); };

    MainWindowSessionRestoreStepExecutor::execute(
        makePlan(MainWindowSessionProjectService::ProjectRestoreStatus::SkippedProtectedFolder,
                 QStringLiteral("/tmp/protected"),
                 {
                     MainWindowSessionRestoreOrchestrationService::Step::AppendSkippedProtectedProjectConsole,
                     MainWindowSessionRestoreOrchestrationService::Step::RefreshTherionConfigDisplay,
                     MainWindowSessionRestoreOrchestrationService::Step::UpdateProjectActionState,
                     MainWindowSessionRestoreOrchestrationService::Step::RestoreOpenDocuments}),
        actions);

    const QStringList expected = {
        QStringLiteral("skipped_console:/tmp/protected"),
        QStringLiteral("refresh_config"),
        QStringLiteral("update_actions"),
        QStringLiteral("restore_docs")};
    if (!expect(calls == expected,
                "Skipped-protected session restore execution order changed unexpectedly.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runExecutionOrderAndPathForwardingTest() != 0) {
        return 1;
    }
    if (runSkippedProtectedExecutionTest() != 0) {
        return 1;
    }

    return 0;
}
