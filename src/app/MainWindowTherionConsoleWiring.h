#pragma once

#include <functional>

#include <QProcess>
#include <QString>

class QLineEdit;
class QComboBox;
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
        QPushButton *therionBrowseTargetConfigButton = nullptr;
        QPushButton *therionBrowseWorkingDirectoryButton = nullptr;
        QPushButton *therionResetWorkingDirectoryButton = nullptr;
        QPushButton *therionClearOutputButton = nullptr;
        QPushButton *therionCopyOutputButton = nullptr;
        QLineEdit *therionWorkingDirectoryEdit = nullptr;
        QLineEdit *therionArgumentsEdit = nullptr;
        QComboBox *therionRunTargetCombo = nullptr;
        QLineEdit *therionTargetConfigEdit = nullptr;
        TherionRunnerService *therionRunnerService = nullptr;

        std::function<void()> onRunRequested;
        std::function<void()> onStopRequested;
        std::function<void()> onBrowseTargetConfigRequested;
        std::function<void()> onBrowseWorkingDirectoryRequested;
        std::function<void()> onResetWorkingDirectoryRequested;
        std::function<void()> onClearOutputRequested;
        std::function<void()> onCopyOutputRequested;
        std::function<void(const QString &)> onWorkingDirectoryChanged;
        std::function<void(const QString &)> onArgumentsChanged;
        std::function<void()> onRunTargetChanged;
        std::function<void(const QString &)> onTargetConfigChanged;
        std::function<void(const QString &)> onRunnerStandardOutput;
        std::function<void(const QString &)> onRunnerStandardError;
        std::function<void(int, QProcess::ExitStatus)> onRunnerFinished;
        std::function<void(const QString &)> onRunnerError;
        std::function<void(bool)> onRunnerStateChanged;
    };

    static void wire(const WiringInput &input);
};
}
