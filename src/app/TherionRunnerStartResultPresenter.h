#pragma once

#include <QString>

#include "TherionRunnerService.h"

namespace TherionStudio
{
class TherionRunnerStartResultPresenter final
{
public:
    struct Presentation
    {
        bool isHandled = false;
        bool appendConsoleMessage = false;
        QString consoleMessage;
        bool showStatusBarMessage = false;
        QString statusBarMessage;
        int statusBarTimeoutMs = 0;
        bool showWarningDialog = false;
        QString warningDialogTitle;
        QString warningDialogMessage;
        bool updateStatusLabel = false;
        QString statusLabelMessage;
    };

    static Presentation present(TherionRunnerService::StartCode code,
                                const QString &executableInput);
};
}
