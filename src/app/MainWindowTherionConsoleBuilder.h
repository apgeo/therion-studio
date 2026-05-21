#pragma once

#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QWidget;

namespace TherionStudio
{
class MainWindowTherionConsoleBuilder final
{
public:
    struct BuildInput
    {
        QWidget *consoleHost = nullptr;
        QString persistedExecutablePath;
        QString suggestedExecutablePath;
        QString persistedWorkingDirectory;
        QString persistedArguments;
    };

    struct BuildResult
    {
        QWidget *rootWidget = nullptr;
        QLineEdit *therionExecutableEdit = nullptr;
        QPushButton *therionBrowseExecutableButton = nullptr;
        QLineEdit *therionWorkingDirectoryEdit = nullptr;
        QLineEdit *therionArgumentsEdit = nullptr;
        QLabel *therionConfigNameValue = nullptr;
        QLabel *therionConfigPathValue = nullptr;
        QLabel *therionRunPolicyLabel = nullptr;
        QLabel *therionStatusLabel = nullptr;
        QPushButton *therionRunButton = nullptr;
        QPushButton *therionStopButton = nullptr;
        QPushButton *therionResetWorkingDirectoryButton = nullptr;
        QPushButton *therionCopyOutputButton = nullptr;
        QPlainTextEdit *consoleView = nullptr;
    };

    static BuildResult build(const BuildInput &input);
};
}
