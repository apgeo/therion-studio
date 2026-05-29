#pragma once

#include "MainWindowProjectLifecycleService.h"

#include <QString>

namespace TherionStudio
{
class MainWindowProjectUiFlowService final
{
public:
    struct DecisionPresentation
    {
        bool shouldContinueWorkflow = false;
        bool showStatusBarMessage = false;
        QString statusBarMessage;
        int statusBarTimeoutMs = 0;
        bool showWarningDialog = false;
        QString warningDialogTitle;
        QString warningDialogMessage;
    };

    struct SuccessPresentation
    {
        QString statusBarMessage;
        int statusBarTimeoutMs = 0;
        QString consoleMessage;
    };

    static DecisionPresentation presentOpenProjectDecision(const MainWindowProjectLifecycleService::OpenProjectDecision &decision);
    static SuccessPresentation presentOpenProjectSuccess(const QString &projectRootPath);

    static DecisionPresentation presentCloseProjectDecision(const MainWindowProjectLifecycleService::CloseProjectDecision &decision);
    static SuccessPresentation presentCloseProjectSuccess(const QString &closedProjectPath);
};
}
