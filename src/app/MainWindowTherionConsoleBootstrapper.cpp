#include "MainWindowTherionConsoleBootstrapper.h"

#include "MainWindowTherionConsoleController.h"

#include <QCoreApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QString translate(const char *sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}
}

namespace TherionStudio
{
void MainWindowTherionConsoleBootstrapper::bootstrap(const BootstrapInput &input)
{
    if (input.consoleController != nullptr) {
        input.consoleController->bindWidgets(input.consoleView,
                                             input.therionRunButton,
                                             input.therionStopButton,
                                             input.therionExecutableEdit,
                                             input.therionBrowseExecutableButton,
                                             input.therionWorkingDirectoryEdit,
                                             input.therionArgumentsEdit);
    }

    if (input.consoleSidebarPageLayout != nullptr && input.rootWidget != nullptr) {
        input.consoleSidebarPageLayout->addWidget(input.rootWidget, 1);
    }

    if (input.refreshTherionConfigDisplay) {
        input.refreshTherionConfigDisplay();
    }

    if (input.updateTherionRunnerState) {
        input.updateTherionRunnerState();
    }
}
}
