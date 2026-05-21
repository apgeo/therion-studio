#include "MainWindow.h"

#include "MainWindowTherionConsoleBootstrapper.h"
#include "MainWindowDocumentHelpers.h"
#include "MainWindowTherionConsoleBuilder.h"
#include "MainWindowTherionConsoleWiring.h"
#include "TherionExecutableSelectionController.h"
#include "TherionRunnerConfigDisplayController.h"
#include "TherionRunnerConfigResolver.h"
#include "TherionRunnerLifecyclePresenter.h"
#include "TherionRunnerService.h"
#include "TherionRunnerStartResultPresenter.h"
#include "TherionRunnerStartSuccessPresenter.h"
#include "../core/SessionStore.h"

#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>

void MainWindow::buildConsole()
{
    QWidget *consoleHost = consoleSidebarPage_ != nullptr ? consoleSidebarPage_ : this;
    const QString persistedExecutablePath = TherionStudio::SessionStore::therionExecutablePath().trimmed();
    const QString detectedHomebrewPath = TherionStudio::TherionRunnerService::suggestedDefaultExecutablePath();
    TherionStudio::MainWindowTherionConsoleBuilder::BuildInput buildInput;
    buildInput.consoleHost = consoleHost;
    buildInput.persistedExecutablePath = persistedExecutablePath;
    buildInput.suggestedExecutablePath = detectedHomebrewPath;
    buildInput.persistedWorkingDirectory = TherionStudio::SessionStore::therionWorkingDirectory().trimmed();
    buildInput.persistedArguments = TherionStudio::SessionStore::therionArguments().trimmed();
    const TherionStudio::MainWindowTherionConsoleBuilder::BuildResult buildResult =
        TherionStudio::MainWindowTherionConsoleBuilder::build(buildInput);

    therionExecutableEdit_ = buildResult.therionExecutableEdit;
    therionBrowseExecutableButton_ = buildResult.therionBrowseExecutableButton;
    therionWorkingDirectoryEdit_ = buildResult.therionWorkingDirectoryEdit;
    therionArgumentsEdit_ = buildResult.therionArgumentsEdit;
    therionConfigNameValue_ = buildResult.therionConfigNameValue;
    therionConfigPathValue_ = buildResult.therionConfigPathValue;
    therionRunPolicyLabel_ = buildResult.therionRunPolicyLabel;
    therionStatusLabel_ = buildResult.therionStatusLabel;
    therionRunButton_ = buildResult.therionRunButton;
    therionStopButton_ = buildResult.therionStopButton;
    therionResetWorkingDirectoryButton_ = buildResult.therionResetWorkingDirectoryButton;
    therionCopyOutputButton_ = buildResult.therionCopyOutputButton;
    consoleView_ = buildResult.consoleView;

    therionRunnerService_ = new TherionStudio::TherionRunnerService(this);
    TherionStudio::MainWindowTherionConsoleWiring::WiringInput wiringInput;
    wiringInput.context = this;
    wiringInput.therionRunButton = therionRunButton_;
    wiringInput.therionStopButton = therionStopButton_;
    wiringInput.therionBrowseExecutableButton = therionBrowseExecutableButton_;
    wiringInput.therionResetWorkingDirectoryButton = therionResetWorkingDirectoryButton_;
    wiringInput.therionCopyOutputButton = therionCopyOutputButton_;
    wiringInput.therionWorkingDirectoryEdit = therionWorkingDirectoryEdit_;
    wiringInput.therionArgumentsEdit = therionArgumentsEdit_;
    wiringInput.therionRunnerService = therionRunnerService_;
    wiringInput.onRunRequested = [this]() { runTherion(); };
    wiringInput.onStopRequested = [this]() { stopTherion(); };
    wiringInput.onBrowseExecutableRequested = [this]() { browseTherionExecutable(); };
    wiringInput.onResetWorkingDirectoryRequested = [this]() {
        therionWorkingDirectoryEdit_->setText(projectRootPath_);
    };
    wiringInput.onCopyOutputRequested = [this]() { copyTherionConsoleOutput(); };
    wiringInput.onWorkingDirectoryChanged = [this](const QString &) { refreshTherionConfigDisplay(); };
    wiringInput.onArgumentsChanged = [this](const QString &) { refreshTherionConfigDisplay(); };
    wiringInput.onRunnerStandardOutput = [this](const QString &output) {
        handleTherionRunnerStandardOutput(output);
    };
    wiringInput.onRunnerStandardError = [this](const QString &output) {
        handleTherionRunnerStandardError(output);
    };
    wiringInput.onRunnerFinished = [this](int exitCode, QProcess::ExitStatus exitStatus) {
        handleTherionRunnerFinished(exitCode, exitStatus);
    };
    wiringInput.onRunnerError = [this](const QString &errorText) { handleTherionRunnerError(errorText); };
    wiringInput.onRunnerStateChanged = [this](bool running) { handleTherionRunnerStateChanged(running); };
    TherionStudio::MainWindowTherionConsoleWiring::wire(wiringInput);

    TherionStudio::MainWindowTherionConsoleBootstrapper::BootstrapInput bootstrapInput;
    bootstrapInput.consoleController = &therionConsoleController_;
    bootstrapInput.consoleView = consoleView_;
    bootstrapInput.therionRunButton = therionRunButton_;
    bootstrapInput.therionStopButton = therionStopButton_;
    bootstrapInput.therionExecutableEdit = therionExecutableEdit_;
    bootstrapInput.therionBrowseExecutableButton = therionBrowseExecutableButton_;
    bootstrapInput.therionWorkingDirectoryEdit = therionWorkingDirectoryEdit_;
    bootstrapInput.therionArgumentsEdit = therionArgumentsEdit_;
    bootstrapInput.consoleSidebarPageLayout = consoleSidebarPageLayout_;
    bootstrapInput.rootWidget = buildResult.rootWidget;
    bootstrapInput.appendConsoleLine = [this](const QString &line) { appendConsoleLine(line); };
    bootstrapInput.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    bootstrapInput.updateTherionRunnerState = [this]() { updateTherionRunnerState(); };
    TherionStudio::MainWindowTherionConsoleBootstrapper::bootstrap(bootstrapInput);
}

void MainWindow::appendConsoleLine(const QString &line)
{
    therionConsoleController_.appendConsoleLine(line);
}

void MainWindow::copyTherionConsoleOutput()
{
    const TherionStudio::MainWindowTherionConsoleController::CopyOutputResult result =
        therionConsoleController_.copyConsoleOutputToClipboard();
    switch (result) {
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::Empty:
        statusBar()->showMessage(tr("Console output is empty"), 2000);
        return;
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::ClipboardUnavailable:
        statusBar()->showMessage(tr("Clipboard is unavailable"), 2000);
        return;
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::ConsoleUnavailable:
        statusBar()->showMessage(tr("Console output is unavailable"), 2000);
        return;
    case TherionStudio::MainWindowTherionConsoleController::CopyOutputResult::Copied:
        break;
    }
    statusBar()->showMessage(tr("Copied console output"), 2000);
}

void MainWindow::browseTherionExecutable()
{
    const QString initialPath = TherionStudio::TherionExecutableSelectionController::initialBrowsePath(
        therionExecutableEdit_ != nullptr ? therionExecutableEdit_->text() : QString());
    const QString selectedExecutablePath = QFileDialog::getOpenFileName(this,
                                                                         tr("Select Therion Executable"),
                                                                         initialPath);
    const TherionStudio::TherionExecutableSelectionController::SelectionResult selectionResult =
        TherionStudio::TherionExecutableSelectionController::evaluateSelection(selectedExecutablePath);
    if (selectionResult.showWarningDialog) {
        QMessageBox::warning(this,
                             selectionResult.warningDialogTitle,
                             selectionResult.warningDialogMessage);
        return;
    }

    if (!selectionResult.isAccepted) {
        return;
    }
    if (selectionResult.shouldUpdateExecutableText && therionExecutableEdit_ != nullptr) {
        therionExecutableEdit_->setText(selectionResult.updatedExecutableText);
    }
    if (selectionResult.shouldShowStatusBarMessage) {
        statusBar()->showMessage(selectionResult.statusBarMessage,
                                 selectionResult.statusBarTimeoutMs);
    }
}

QString MainWindow::resolvedTherionWorkingDirectory() const
{
    return TherionStudio::TherionRunnerConfigResolver::resolveWorkingDirectory(
        therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text().trimmed() : QString(),
        projectRootPath_);
}

bool MainWindow::hasExplicitTherionConfigArgument() const
{
    return TherionStudio::TherionRunnerConfigResolver::hasExplicitConfigArgument(
        QProcess::splitCommand(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString()));
}

QString MainWindow::resolvedTherionConfigPath() const
{
    QWidget *activeWidget = currentDocumentWidget();
    return TherionStudio::TherionRunnerConfigResolver::resolveConfigPath(
        QProcess::splitCommand(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString()),
        resolvedTherionWorkingDirectory(),
        projectRootPath_,
        activeWidget != nullptr ? documentPathForWidget(activeWidget) : QString());
}

void MainWindow::refreshTherionConfigDisplay()
{
    if (therionConfigNameValue_ == nullptr || therionConfigPathValue_ == nullptr) {
        return;
    }

    const bool explicitConfigArgument = hasExplicitTherionConfigArgument();
    const QString resolvedPath = resolvedTherionConfigPath();
    const TherionStudio::TherionRunnerConfigDisplayController::RefreshComputation computation =
        TherionStudio::TherionRunnerConfigDisplayController::compute(
            resolvedPath,
            explicitConfigArgument,
            therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text() : QString(),
            projectRootPath_,
            autoResolvedTherionWorkingDirectory_);

    if (!computation.hasResolvedConfigPath) {
        therionConfigNameValue_->setText(tr("Auto-detect"));
        therionConfigPathValue_->setText(tr("No config file resolved from the current context"));
        therionConfigPathValue_->setToolTip(QString());
        if (computation.shouldClearAutoResolvedWorkingDirectory) {
            autoResolvedTherionWorkingDirectory_.clear();
        }
        return;
    }

    therionConfigNameValue_->setText(computation.configName.isEmpty() ? tr("thconfig") : computation.configName);
    therionConfigPathValue_->setText(computation.configPath);
    therionConfigPathValue_->setToolTip(computation.configPath);

    if (therionWorkingDirectoryEdit_ != nullptr && computation.shouldUpdateWorkingDirectoryText) {
        therionWorkingDirectoryEdit_->setText(computation.updatedWorkingDirectoryText);
    }
    if (computation.shouldUpdateAutoResolvedWorkingDirectory) {
        autoResolvedTherionWorkingDirectory_ = computation.updatedAutoResolvedWorkingDirectory;
    }
}

void MainWindow::updateTherionRunnerState()
{
    const bool isRunning = therionRunnerService_ != nullptr && therionRunnerService_->isRunning();
    therionConsoleController_.applyRunnerState(isRunning);
}

void MainWindow::runTherion()
{
    if (therionRunnerService_ == nullptr) {
        appendConsoleLine(tr("Therion runner is unavailable."));
        return;
    }

    const QString workingDirectory = resolvedTherionWorkingDirectory();
    const QString executableInput = therionExecutableEdit_ != nullptr && !therionExecutableEdit_->text().trimmed().isEmpty()
                                        ? therionExecutableEdit_->text().trimmed()
                                        : QStringLiteral("therion");
    const QString argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString();
    const QStringList arguments = argumentsText.isEmpty() ? QStringList() : QProcess::splitCommand(argumentsText);
    const TherionStudio::TherionRunnerService::StartResult startResult =
        therionRunnerService_->start(executableInput, workingDirectory, arguments);
    const TherionStudio::TherionRunnerStartResultPresenter::Presentation startPresentation =
        TherionStudio::TherionRunnerStartResultPresenter::present(startResult.code, executableInput);

    if (startPresentation.isHandled) {
        if (startPresentation.showWarningDialog) {
            QMessageBox::warning(this,
                                 startPresentation.warningDialogTitle,
                                 startPresentation.warningDialogMessage);
        }
        if (startPresentation.appendConsoleMessage) {
            appendConsoleLine(startPresentation.consoleMessage);
        }
        if (startPresentation.showStatusBarMessage) {
            statusBar()->showMessage(startPresentation.statusBarMessage,
                                     startPresentation.statusBarTimeoutMs);
        }
        if (startPresentation.updateStatusLabel && therionStatusLabel_ != nullptr) {
            therionStatusLabel_->setText(startPresentation.statusLabelMessage);
        }
        return;
    }

    const TherionStudio::TherionRunnerStartSuccessPresenter::Presentation successPresentation =
        TherionStudio::TherionRunnerStartSuccessPresenter::present(startResult,
                                                                   executableInput,
                                                                   argumentsText,
                                                                   workingDirectory);
    if (successPresentation.shouldUpdateExecutableText && therionExecutableEdit_ != nullptr) {
        therionExecutableEdit_->setText(successPresentation.updatedExecutableText);
    }

    appendConsoleLine(successPresentation.consoleMessage);
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(successPresentation.statusLabelMessage);
    }
    updateTherionRunnerState();
}

void MainWindow::stopTherion()
{
    const TherionStudio::TherionRunnerLifecyclePresenter::StopPresentation stopPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentStopRequest(
            therionRunnerService_ != nullptr && therionRunnerService_->isRunning());
    appendConsoleLine(stopPresentation.consoleMessage);

    if (!stopPresentation.shouldStopProcess || therionRunnerService_ == nullptr) {
        return;
    }

    therionRunnerService_->stop();
    if (stopPresentation.shouldUpdateStatusLabel && therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(stopPresentation.statusLabelMessage);
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionRunnerStandardOutput(const QString &output)
{
    therionConsoleController_.appendProcessStandardOutput(output);
}

void MainWindow::handleTherionRunnerStandardError(const QString &output)
{
    therionConsoleController_.appendProcessStandardError(output, tr("[stderr] %1"));
}

void MainWindow::handleTherionRunnerFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const TherionStudio::TherionRunnerLifecyclePresenter::EventPresentation eventPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentFinished(exitCode, exitStatus);
    appendConsoleLine(eventPresentation.statusText);
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(eventPresentation.statusText);
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionRunnerError(const QString &errorText)
{
    const TherionStudio::TherionRunnerLifecyclePresenter::EventPresentation eventPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentError(errorText);
    appendConsoleLine(eventPresentation.statusText);
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(eventPresentation.statusText);
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionRunnerStateChanged(bool)
{
    updateTherionRunnerState();
}
