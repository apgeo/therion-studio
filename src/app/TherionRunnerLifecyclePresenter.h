#pragma once

#include <QProcess>
#include <QString>

namespace TherionStudio
{
class TherionRunnerLifecyclePresenter final
{
public:
    struct StopPresentation
    {
        bool shouldStopProcess = false;
        QString consoleMessage;
        bool shouldUpdateStatusLabel = false;
        QString statusLabelMessage;
    };

    struct EventPresentation
    {
        QString statusText;
    };

    static StopPresentation presentStopRequest(bool isRunning);
    static EventPresentation presentFinished(int exitCode, QProcess::ExitStatus exitStatus);
    static EventPresentation presentError(const QString &errorText);
};
}
