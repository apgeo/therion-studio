#pragma once

#include <functional>

#include <QString>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;
class QWidget;

namespace TherionStudio
{
class MainWindowTherionConsoleController;

class MainWindowTherionConsoleBootstrapper final
{
public:
    struct BootstrapInput
    {
        MainWindowTherionConsoleController *consoleController = nullptr;
        QPlainTextEdit *consoleView = nullptr;
        QPushButton *therionRunButton = nullptr;
        QPushButton *therionStopButton = nullptr;
        QLineEdit *therionWorkingDirectoryEdit = nullptr;
        QLineEdit *therionArgumentsEdit = nullptr;
        QVBoxLayout *consoleSidebarPageLayout = nullptr;
        QWidget *rootWidget = nullptr;

        std::function<void(const QString &)> appendConsoleLine;
        std::function<void()> refreshTherionConfigDisplay;
        std::function<void()> updateTherionRunnerState;
    };

    static void bootstrap(const BootstrapInput &input);
};
}
