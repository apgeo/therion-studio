#include "../src/app/MainWindowProjectLifecycleService.h"

#include <QDir>
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

int runOpenProjectDecisionTest()
{
    const auto cancelled = MainWindowProjectLifecycleService::decideOpenProject(QString());
    if (!expect(cancelled.status == MainWindowProjectLifecycleService::OpenProjectStatus::Cancelled,
                "Empty selection should be treated as cancelled open-project request.")) {
        return 1;
    }

    const auto missing =
        MainWindowProjectLifecycleService::decideOpenProject(QStringLiteral("/definitely/missing/project/path"));
    if (!expect(missing.status == MainWindowProjectLifecycleService::OpenProjectStatus::MissingDirectory,
                "Missing directory should fail open-project decision.")) {
        return 1;
    }

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const auto open = MainWindowProjectLifecycleService::decideOpenProject(tempDir.path());
    if (!expect(open.status == MainWindowProjectLifecycleService::OpenProjectStatus::OpenProject,
                "Existing directory should pass open-project decision.")) {
        return 1;
    }
    if (!expect(open.projectPath == QDir(tempDir.path()).absolutePath(),
                "Open-project decision should normalize path to absolute path.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectDecisionTest()
{
    const auto noProject = MainWindowProjectLifecycleService::decideCloseProject(QString(), true);
    if (!expect(noProject.status == MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen,
                "Empty current project should be treated as no-open-project state.")) {
        return 1;
    }

    const auto cancelled = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"), false);
    if (!expect(cancelled.status == MainWindowProjectLifecycleService::CloseProjectStatus::CancelledByDirtyDocuments,
                "Dirty-document veto should cancel close-project decision.")) {
        return 1;
    }

    const auto close = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"), true);
    if (!expect(close.status == MainWindowProjectLifecycleService::CloseProjectStatus::CloseProject,
                "Close-project decision should proceed when dirty docs can close.")) {
        return 1;
    }
    if (!expect(close.closedProjectPath == QStringLiteral("/tmp/project"),
                "Close-project decision should preserve closed project path for logging/UI.")) {
        return 1;
    }

    return 0;
}

int runCloseProjectCallbackDecisionTest()
{
    int callbackCallCount = 0;
    const auto noProject = MainWindowProjectLifecycleService::decideCloseProject(QString(), [&callbackCallCount]() {
        ++callbackCallCount;
        return true;
    });
    if (!expect(noProject.status == MainWindowProjectLifecycleService::CloseProjectStatus::NoProjectOpen,
                "Callback close decision should keep no-project state for empty project path.")) {
        return 1;
    }
    if (!expect(callbackCallCount == 0,
                "Close-project confirmation callback should not be called when no project is open.")) {
        return 1;
    }

    callbackCallCount = 0;
    const auto cancelled = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"),
                                                                                  [&callbackCallCount]() {
                                                                                      ++callbackCallCount;
                                                                                      return false;
                                                                                  });
    if (!expect(cancelled.status == MainWindowProjectLifecycleService::CloseProjectStatus::CancelledByDirtyDocuments,
                "Callback close decision should cancel when confirmation callback returns false.")) {
        return 1;
    }
    if (!expect(callbackCallCount == 1,
                "Close-project confirmation callback should be called once for open projects.")) {
        return 1;
    }

    callbackCallCount = 0;
    const auto close = MainWindowProjectLifecycleService::decideCloseProject(QStringLiteral("/tmp/project"),
                                                                              [&callbackCallCount]() {
                                                                                  ++callbackCallCount;
                                                                                  return true;
                                                                              });
    if (!expect(close.status == MainWindowProjectLifecycleService::CloseProjectStatus::CloseProject,
                "Callback close decision should proceed when confirmation callback returns true.")) {
        return 1;
    }
    if (!expect(callbackCallCount == 1,
                "Close-project confirmation callback should be called once for close success.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runOpenProjectDecisionTest() != 0) {
        return 1;
    }
    if (runCloseProjectDecisionTest() != 0) {
        return 1;
    }
    if (runCloseProjectCallbackDecisionTest() != 0) {
        return 1;
    }

    return 0;
}
