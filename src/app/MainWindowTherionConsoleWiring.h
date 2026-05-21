#pragma once

#include <functional>

#include <QProcess>
#include <QString>

class QLineEdit;
class QObject;
class QPushButton;

namespace TherionStudio
{
class TherionRunnerService;

class MainWindowTherionConsoleWiring final
{
public:
    struct WiringInput
    {
        QObject *context = nullptr;
        QPushButton *therionRunButton = nullptr;
        QPushButton *therionStopButton = nullptr;
        QPushButton *therionBrowseExecutableButton = nullptr;
        QPushButton *therionResetWorkingDirectoryButton = nullptr;
        QPushButton *therionCopyOutputButton = nullptr;
        QLineEdit *therionWorkingDirectoryEdit = nullptr;
        QLineEdit *therionArgumentsEdit = nullptr;
        TherionRunnerService *therionRunnerService = nullptr;

        std::function<void()> onRunRequested;
        std::function<void()> onStopRequested;
        std::function<void()> onBrowseExecutableRequested;
        std::function<void()> onResetWorkingDirectoryRequested;
        std::function<void()> onCopyOutputRequested;
        std::function<void(const QString &)> onWorkingDirectoryChanged;
        std::function<void(const QString &)> onArgumentsChanged;
        std::function<void(const QString &)> onRunnerStandardOutput;
        std::function<void(const QString &)> onRunnerStandardError;
        std::function<void(int, QProcess::ExitStatus)> onRunnerFinished;
        std::function<void(const QString &)> onRunnerError;
        std::function<void(bool)> onRunnerStateChanged;
    };

    static void wire(const WiringInput &input);
};
}
