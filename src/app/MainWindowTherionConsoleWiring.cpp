#include "MainWindowTherionConsoleWiring.h"

#include "TherionRunnerService.h"

#include <QComboBox>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>

namespace TherionStudio
{
void MainWindowTherionConsoleWiring::wire(const WiringInput &input)
{
    if (input.context == nullptr) {
        return;
    }

    if (input.therionRunButton != nullptr) {
        QObject::connect(input.therionRunButton, &QPushButton::clicked, input.context, [handler = input.onRunRequested]() {
            if (handler) {
                handler();
            }
        });
    }

    if (input.therionStopButton != nullptr) {
        QObject::connect(input.therionStopButton, &QPushButton::clicked, input.context, [handler = input.onStopRequested]() {
            if (handler) {
                handler();
            }
        });
    }

    if (input.therionBrowseTargetConfigButton != nullptr) {
        QObject::connect(input.therionBrowseTargetConfigButton,
                         &QPushButton::clicked,
                         input.context,
                         [handler = input.onBrowseTargetConfigRequested]() {
                             if (handler) {
                                 handler();
                             }
                         });
    }

    if (input.therionBrowseWorkingDirectoryButton != nullptr) {
        QObject::connect(input.therionBrowseWorkingDirectoryButton,
                         &QPushButton::clicked,
                         input.context,
                         [handler = input.onBrowseWorkingDirectoryRequested]() {
                             if (handler) {
                                 handler();
                             }
                         });
    }

    if (input.therionResetWorkingDirectoryButton != nullptr) {
        QObject::connect(input.therionResetWorkingDirectoryButton,
                         &QPushButton::clicked,
                         input.context,
                         [handler = input.onResetWorkingDirectoryRequested]() {
                             if (handler) {
                                 handler();
                             }
                         });
    }

    if (input.therionCopyOutputButton != nullptr) {
        QObject::connect(input.therionCopyOutputButton,
                         &QPushButton::clicked,
                         input.context,
                         [handler = input.onCopyOutputRequested]() {
                             if (handler) {
                                 handler();
                             }
                         });
    }

    if (input.therionClearOutputButton != nullptr) {
        QObject::connect(input.therionClearOutputButton,
                         &QPushButton::clicked,
                         input.context,
                         [handler = input.onClearOutputRequested]() {
                             if (handler) {
                                 handler();
                             }
                         });
    }

    if (input.therionWorkingDirectoryEdit != nullptr) {
        QObject::connect(input.therionWorkingDirectoryEdit,
                         &QLineEdit::textChanged,
                         input.context,
                         [handler = input.onWorkingDirectoryChanged](const QString &text) {
                             if (handler) {
                                 handler(text);
                             }
                         });
    }

    if (input.therionArgumentsEdit != nullptr) {
        QObject::connect(input.therionArgumentsEdit,
                         &QLineEdit::textChanged,
                         input.context,
                         [handler = input.onArgumentsChanged](const QString &text) {
                             if (handler) {
                                 handler(text);
                             }
                         });
    }

    if (input.therionRunTargetCombo != nullptr) {
        QObject::connect(input.therionRunTargetCombo,
                         &QComboBox::currentIndexChanged,
                         input.context,
                         [handler = input.onRunTargetChanged](int) {
                             if (handler) {
                                 handler();
                             }
                         });
    }

    if (input.therionTargetConfigEdit != nullptr) {
        QObject::connect(input.therionTargetConfigEdit,
                         &QLineEdit::textChanged,
                         input.context,
                         [handler = input.onTargetConfigChanged](const QString &text) {
                             if (handler) {
                                 handler(text);
                             }
                         });
    }

    if (input.therionRunnerService != nullptr) {
        QObject::connect(input.therionRunnerService,
                         &TherionRunnerService::standardOutputReady,
                         input.context,
                         [handler = input.onRunnerStandardOutput](const QString &output) {
                             if (handler) {
                                 handler(output);
                             }
                         });
        QObject::connect(input.therionRunnerService,
                         &TherionRunnerService::standardErrorReady,
                         input.context,
                         [handler = input.onRunnerStandardError](const QString &output) {
                             if (handler) {
                                 handler(output);
                             }
                         });
        QObject::connect(input.therionRunnerService,
                         &TherionRunnerService::runFinished,
                         input.context,
                         [handler = input.onRunnerFinished](int exitCode, QProcess::ExitStatus exitStatus) {
                             if (handler) {
                                 handler(exitCode, exitStatus);
                             }
                         });
        QObject::connect(input.therionRunnerService,
                         &TherionRunnerService::runError,
                         input.context,
                         [handler = input.onRunnerError](const QString &errorText) {
                             if (handler) {
                                 handler(errorText);
                             }
                         });
        QObject::connect(input.therionRunnerService,
                         &TherionRunnerService::runningStateChanged,
                         input.context,
                         [handler = input.onRunnerStateChanged](bool running) {
                             if (handler) {
                                 handler(running);
                             }
                         });
    }
}
}
