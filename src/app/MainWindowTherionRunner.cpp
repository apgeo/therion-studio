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

#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>

namespace
{
constexpr auto kTherionRunTargetCurrent = "current";
constexpr auto kTherionRunTargetProject = "project";

bool isTherionConfigPath(const QString &path)
{
    const QFileInfo fileInfo(path);
    return fileInfo.fileName().compare(QStringLiteral("thconfig"), Qt::CaseInsensitive) == 0
           || fileInfo.suffix().compare(QStringLiteral("thconfig"), Qt::CaseInsensitive) == 0;
}

QString canonicalOrAbsolutePath(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        return QString();
    }

    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}
}

void MainWindow::buildConsole()
{
    QWidget *consoleHost = consoleSidebarPage_ != nullptr ? consoleSidebarPage_ : this;
    const QString persistedExecutablePath = sessionStore_->therionExecutablePath().trimmed();
    const QString suggestedExecutablePath = TherionStudio::TherionRunnerService::suggestedDefaultExecutablePath();
    TherionStudio::MainWindowTherionConsoleBuilder::BuildInput buildInput;
    buildInput.consoleHost = consoleHost;
    buildInput.persistedExecutablePath = persistedExecutablePath;
    buildInput.suggestedExecutablePath = suggestedExecutablePath;
    buildInput.persistedWorkingDirectory = sessionStore_->therionWorkingDirectory().trimmed();
    buildInput.persistedArguments = sessionStore_->therionArguments().trimmed();
    buildInput.persistedRunTargetMode = sessionStore_->therionRunTargetMode().trimmed();
    buildInput.persistedTargetConfigPath = sessionStore_->therionTargetConfigPath().trimmed();
    const TherionStudio::MainWindowTherionConsoleBuilder::BuildResult buildResult =
        TherionStudio::MainWindowTherionConsoleBuilder::build(buildInput);

    therionExecutableEdit_ = buildResult.therionExecutableEdit;
    therionBrowseExecutableButton_ = buildResult.therionBrowseExecutableButton;
    therionWorkingDirectoryEdit_ = buildResult.therionWorkingDirectoryEdit;
    therionBrowseWorkingDirectoryButton_ = buildResult.therionBrowseWorkingDirectoryButton;
    therionArgumentsEdit_ = buildResult.therionArgumentsEdit;
    therionRunTargetCombo_ = buildResult.therionRunTargetCombo;
    therionTargetConfigEdit_ = buildResult.therionTargetConfigEdit;
    therionBrowseTargetConfigButton_ = buildResult.therionBrowseTargetConfigButton;
    therionConfigNameValue_ = buildResult.therionConfigNameValue;
    therionConfigPathValue_ = buildResult.therionConfigPathValue;
    therionWorkingDirectoryValue_ = buildResult.therionWorkingDirectoryValue;
    therionRunButton_ = buildResult.therionRunButton;
    therionStopButton_ = buildResult.therionStopButton;
    therionResetWorkingDirectoryButton_ = buildResult.therionResetWorkingDirectoryButton;
    therionClearOutputButton_ = buildResult.therionClearOutputButton;
    therionCopyOutputButton_ = buildResult.therionCopyOutputButton;
    consoleView_ = buildResult.consoleView;

    therionRunnerService_ = new TherionStudio::TherionRunnerService(this);
    TherionStudio::MainWindowTherionConsoleWiring::WiringInput wiringInput;
    wiringInput.context = this;
    wiringInput.therionRunButton = therionRunButton_;
    wiringInput.therionStopButton = therionStopButton_;
    wiringInput.therionBrowseExecutableButton = therionBrowseExecutableButton_;
    wiringInput.therionBrowseTargetConfigButton = therionBrowseTargetConfigButton_;
    wiringInput.therionBrowseWorkingDirectoryButton = therionBrowseWorkingDirectoryButton_;
    wiringInput.therionResetWorkingDirectoryButton = therionResetWorkingDirectoryButton_;
    wiringInput.therionClearOutputButton = therionClearOutputButton_;
    wiringInput.therionCopyOutputButton = therionCopyOutputButton_;
    wiringInput.therionWorkingDirectoryEdit = therionWorkingDirectoryEdit_;
    wiringInput.therionArgumentsEdit = therionArgumentsEdit_;
    wiringInput.therionRunTargetCombo = therionRunTargetCombo_;
    wiringInput.therionTargetConfigEdit = therionTargetConfigEdit_;
    wiringInput.therionRunnerService = therionRunnerService_;
    wiringInput.onRunRequested = [this]() { runTherion(); };
    wiringInput.onStopRequested = [this]() { stopTherion(); };
    wiringInput.onBrowseExecutableRequested = [this]() { browseTherionExecutable(); };
    wiringInput.onBrowseTargetConfigRequested = [this]() { browseTherionTargetConfig(); };
    wiringInput.onBrowseWorkingDirectoryRequested = [this]() { browseTherionWorkingDirectoryOverride(); };
    wiringInput.onResetWorkingDirectoryRequested = [this]() {
        therionWorkingDirectoryEdit_->clear();
    };
    wiringInput.onClearOutputRequested = [this]() { clearTherionConsoleOutput(); };
    wiringInput.onCopyOutputRequested = [this]() { copyTherionConsoleOutput(); };
    wiringInput.onWorkingDirectoryChanged = [this](const QString &) { refreshTherionConfigDisplay(); };
    wiringInput.onArgumentsChanged = [this](const QString &) { refreshTherionConfigDisplay(); };
    wiringInput.onRunTargetChanged = [this]() { refreshTherionConfigDisplay(); };
    wiringInput.onTargetConfigChanged = [this](const QString &) { refreshTherionConfigDisplay(); };
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
    bootstrapInput.refreshTherionConfigDisplay = [this]() { refreshTherionConfigDisplay(); };
    bootstrapInput.updateTherionRunnerState = [this]() { updateTherionRunnerState(); };
    TherionStudio::MainWindowTherionConsoleBootstrapper::bootstrap(bootstrapInput);
}

void MainWindow::appendConsoleLine(const QString &line)
{
    if (line.trimmed().isEmpty() || statusBar() == nullptr) {
        return;
    }
    statusBar()->showMessage(line, 3000);
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

void MainWindow::clearTherionConsoleOutput()
{
    if (!therionConsoleController_.clearConsoleOutput()) {
        statusBar()->showMessage(tr("Console output is unavailable"), 2000);
        return;
    }

    statusBar()->showMessage(tr("Cleared console output"), 2000);
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

void MainWindow::browseTherionTargetConfig()
{
    QString initialPath = therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text().trimmed() : QString();
    if (initialPath.isEmpty()) {
        initialPath = resolvedTherionConfigPath();
    }
    if (initialPath.isEmpty()) {
        initialPath = therionConfigResolutionDirectory();
    }
    if (initialPath.isEmpty()) {
        initialPath = projectRootPath_;
    }

    const QString selectedConfigPath =
        QFileDialog::getOpenFileName(this,
                                     tr("Select Therion Config"),
                                     initialPath,
                                     tr("Therion config files (thconfig *.thconfig);;All files (*)"));
    if (selectedConfigPath.isEmpty() || therionTargetConfigEdit_ == nullptr) {
        return;
    }

    if (therionRunTargetCombo_ != nullptr) {
        const int projectIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetProject));
        if (projectIndex >= 0) {
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
    }
    therionTargetConfigEdit_->setText(selectedConfigPath);
    therionTargetConfigEdit_->setCursorPosition(therionTargetConfigEdit_->text().size());
}

void MainWindow::browseTherionWorkingDirectoryOverride()
{
    QString initialPath = therionWorkingDirectoryEdit_ != nullptr
        ? therionWorkingDirectoryEdit_->text().trimmed()
        : QString();
    if (initialPath.isEmpty()) {
        initialPath = resolvedTherionWorkingDirectory();
    }
    if (initialPath.isEmpty()) {
        initialPath = projectRootPath_;
    }

    const QString selectedDirectory =
        QFileDialog::getExistingDirectory(this,
                                          tr("Select Working Directory Override"),
                                          initialPath);
    if (selectedDirectory.isEmpty() || therionWorkingDirectoryEdit_ == nullptr) {
        return;
    }

    therionWorkingDirectoryEdit_->setText(selectedDirectory);
    therionWorkingDirectoryEdit_->setCursorPosition(therionWorkingDirectoryEdit_->text().size());
}

QString MainWindow::therionWorkingDirectoryOverride() const
{
    if (therionRunTargetMode() == QString::fromLatin1(kTherionRunTargetCurrent)) {
        return QString();
    }

    return therionWorkingDirectoryEdit_ != nullptr
        ? therionWorkingDirectoryEdit_->text().trimmed()
        : QString();
}

QString MainWindow::therionConfigResolutionDirectory() const
{
    return TherionStudio::TherionRunnerConfigResolver::resolveWorkingDirectory(
        therionWorkingDirectoryOverride(),
        projectRootPath_);
}

QString MainWindow::resolvedTherionWorkingDirectory() const
{
    const QString overrideDirectory = therionWorkingDirectoryOverride();
    if (!overrideDirectory.isEmpty()) {
        return TherionStudio::TherionRunnerConfigResolver::resolveWorkingDirectory(overrideDirectory,
                                                                                  projectRootPath_);
    }

    if (hasExplicitTherionConfigArgument()) {
        return therionConfigResolutionDirectory();
    }

    const QString resolvedConfigPath = resolvedTherionConfigPath();
    if (!resolvedConfigPath.isEmpty()) {
        const QFileInfo configInfo(resolvedConfigPath);
        return configInfo.absolutePath();
    }

    return TherionStudio::TherionRunnerConfigResolver::resolveWorkingDirectory(QString(), projectRootPath_);
}

bool MainWindow::hasExplicitTherionConfigArgument() const
{
    return TherionStudio::TherionRunnerConfigResolver::hasExplicitConfigArgument(
        QProcess::splitCommand(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString()));
}

QString MainWindow::therionRunTargetMode() const
{
    if (therionRunTargetCombo_ == nullptr) {
        return QString::fromLatin1(kTherionRunTargetProject);
    }

    const QString mode = therionRunTargetCombo_->currentData().toString().trimmed();
    if (mode == QString::fromLatin1(kTherionRunTargetCurrent)
        || mode == QString::fromLatin1(kTherionRunTargetProject)) {
        return mode;
    }

    return QString::fromLatin1(kTherionRunTargetProject);
}

QString MainWindow::currentDocumentTherionConfigPath() const
{
    QWidget *activeWidget = currentDocumentWidget();
    const QString activeDocumentPath =
        activeWidget != nullptr ? documentPathForWidget(activeWidget) : QString();
    if (activeDocumentPath.isEmpty() || !isTherionConfigPath(activeDocumentPath)) {
        return QString();
    }

    return canonicalOrAbsolutePath(activeDocumentPath);
}

QString MainWindow::resolvedTherionTargetConfigPath() const
{
    const QString targetPath =
        therionTargetConfigEdit_ != nullptr ? therionTargetConfigEdit_->text().trimmed() : QString();
    if (targetPath.isEmpty()) {
        return QString();
    }

    return TherionStudio::TherionRunnerConfigResolver::resolveCandidatePath(targetPath,
                                                                            therionConfigResolutionDirectory(),
                                                                            projectRootPath_);
}

QString MainWindow::resolvedTherionConfigPath() const
{
    const QStringList arguments =
        QProcess::splitCommand(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString());
    const QString workingDirectory = therionConfigResolutionDirectory();
    if (TherionStudio::TherionRunnerConfigResolver::hasExplicitConfigArgument(arguments)) {
        return TherionStudio::TherionRunnerConfigResolver::resolveConfigPath(arguments,
                                                                            workingDirectory,
                                                                            projectRootPath_,
                                                                            currentDocumentTherionConfigPath());
    }

    const QString mode = therionRunTargetMode();
    if (mode == QString::fromLatin1(kTherionRunTargetCurrent)) {
        return currentDocumentTherionConfigPath();
    }

    const QString targetConfigPath = resolvedTherionTargetConfigPath();
    if (!targetConfigPath.isEmpty()) {
        return targetConfigPath;
    }

    if (mode == QString::fromLatin1(kTherionRunTargetProject)) {
        return TherionStudio::TherionRunnerConfigResolver::resolveConfigPath(QStringList(),
                                                                            workingDirectory,
                                                                            projectRootPath_,
                                                                            QString());
    }

    return QString();
}

QStringList MainWindow::therionArgumentsForRun(const QStringList &baseArguments,
                                               const QString &resolvedConfigPath) const
{
    if (TherionStudio::TherionRunnerConfigResolver::hasExplicitConfigArgument(baseArguments)
        || resolvedConfigPath.isEmpty()) {
        return baseArguments;
    }

    QStringList arguments = baseArguments;
    arguments.append(resolvedConfigPath);
    return arguments;
}

void MainWindow::refreshTherionConfigDisplay()
{
    if (therionConfigNameValue_ == nullptr || therionConfigPathValue_ == nullptr) {
        return;
    }

    refreshTherionRunTargetControls();

    const QString resolvedPath = resolvedTherionConfigPath();
    const TherionStudio::TherionRunnerConfigDisplayController::RefreshComputation computation =
        TherionStudio::TherionRunnerConfigDisplayController::compute(
            resolvedPath);
    const QString workingDirectory = resolvedTherionWorkingDirectory();
    if (therionWorkingDirectoryValue_ != nullptr) {
        therionWorkingDirectoryValue_->setText(workingDirectory.isEmpty()
                                                   ? tr("No working directory resolved")
                                                   : workingDirectory);
        therionWorkingDirectoryValue_->setToolTip(workingDirectory);
    }

    if (!computation.hasResolvedConfigPath) {
        therionConfigNameValue_->setText(tr("Auto-detect"));
        therionConfigPathValue_->setText(tr("No config file resolved from the current context"));
        therionConfigPathValue_->setToolTip(QString());
        return;
    }

    therionConfigNameValue_->setText(computation.configName.isEmpty() ? tr("thconfig") : computation.configName);
    therionConfigPathValue_->setText(computation.configPath);
    therionConfigPathValue_->setToolTip(computation.configPath);

}

void MainWindow::refreshTherionRunTargetControls()
{
    const bool currentConfigAvailable = !currentDocumentTherionConfigPath().isEmpty();
    if (therionRunTargetCombo_ != nullptr) {
        const int currentIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetCurrent));
        const int projectIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetProject));
        if (currentConfigAvailable && currentIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(currentIndex);
        } else if (!currentConfigAvailable && projectIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
        therionRunTargetCombo_->setEnabled(currentConfigAvailable);
        therionRunTargetCombo_->setToolTip(
            currentConfigAvailable
                ? QString()
                : tr("Open a thconfig or .thconfig tab to use Current Config."));
    }

    const bool projectConfigActive = therionRunTargetMode() != QString::fromLatin1(kTherionRunTargetCurrent);
    if (therionTargetConfigEdit_ != nullptr) {
        therionTargetConfigEdit_->setEnabled(projectConfigActive);
    }
    if (therionBrowseTargetConfigButton_ != nullptr) {
        therionBrowseTargetConfigButton_->setEnabled(projectConfigActive);
    }
    if (therionWorkingDirectoryEdit_ != nullptr) {
        therionWorkingDirectoryEdit_->setEnabled(projectConfigActive);
        therionWorkingDirectoryEdit_->setToolTip(
            projectConfigActive
                ? QString()
                : tr("Current Config always runs from the active config file folder."));
    }
    if (therionBrowseWorkingDirectoryButton_ != nullptr) {
        therionBrowseWorkingDirectoryButton_->setEnabled(projectConfigActive);
    }
    if (therionResetWorkingDirectoryButton_ != nullptr) {
        const bool hasOverride = therionWorkingDirectoryEdit_ != nullptr
            && !therionWorkingDirectoryEdit_->text().trimmed().isEmpty();
        therionResetWorkingDirectoryButton_->setEnabled(projectConfigActive && hasOverride);
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
        statusBar()->showMessage(tr("Therion runner is unavailable."), 3000);
        setCompilerStatusResult(false, tr("Therion runner is unavailable."));
        return;
    }

    const QString workingDirectory = resolvedTherionWorkingDirectory();
    const QString executableInput = therionExecutableEdit_ != nullptr && !therionExecutableEdit_->text().trimmed().isEmpty()
                                        ? therionExecutableEdit_->text().trimmed()
                                        : QStringLiteral("therion");
    const QString argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString();
    const QStringList baseArguments = argumentsText.isEmpty() ? QStringList() : QProcess::splitCommand(argumentsText);
    const QString resolvedConfigPath = resolvedTherionConfigPath();
    const QStringList arguments = therionArgumentsForRun(baseArguments, resolvedConfigPath);
    if (!TherionStudio::TherionRunnerConfigResolver::hasExplicitConfigArgument(baseArguments)
        && resolvedConfigPath.isEmpty()) {
        const QString message = tr("Select a current or project Therion config before running Therion.");
        QMessageBox::warning(this, tr("Run Therion"), message);
        setCompilerStatusResult(false, message);
        return;
    }

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
        if (startPresentation.showStatusBarMessage) {
            statusBar()->showMessage(startPresentation.statusBarMessage,
                                     startPresentation.statusBarTimeoutMs);
        }
        if (startResult.code != TherionStudio::TherionRunnerService::StartCode::AlreadyRunning) {
            setCompilerStatusResult(false,
                                    startPresentation.statusLabelMessage.isEmpty()
                                        ? startPresentation.warningDialogMessage
                                        : startPresentation.statusLabelMessage);
        }
        return;
    }

    activeTherionRunConfigPath_ = resolvedConfigPath;
    setCompilerStatusRunning(activeTherionRunConfigPath_);

    updateTherionRunnerState();
}

void MainWindow::runTherionProjectConfig()
{
    if (therionRunTargetCombo_ != nullptr) {
        const int projectIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetProject));
        if (projectIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(projectIndex);
        }
    }

    runTherion();
    refreshTherionConfigDisplay();
}

void MainWindow::runTherionCurrentConfig()
{
    if (currentDocumentTherionConfigPath().isEmpty()) {
        const QString message = tr("Open a thconfig or .thconfig tab before compiling the current config.");
        QMessageBox::warning(this, tr("Run Therion"), message);
        setCompilerStatusResult(false, message);
        return;
    }

    if (therionRunTargetCombo_ != nullptr) {
        const int currentIndex =
            therionRunTargetCombo_->findData(QString::fromLatin1(kTherionRunTargetCurrent));
        if (currentIndex >= 0) {
            const QSignalBlocker blocker(therionRunTargetCombo_);
            therionRunTargetCombo_->setCurrentIndex(currentIndex);
        }
    }

    runTherion();
    refreshTherionConfigDisplay();
}

void MainWindow::stopTherion()
{
    const TherionStudio::TherionRunnerLifecyclePresenter::StopPresentation stopPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentStopRequest(
            therionRunnerService_ != nullptr && therionRunnerService_->isRunning());

    if (!stopPresentation.shouldStopProcess || therionRunnerService_ == nullptr) {
        if (stopPresentation.shouldUpdateStatusLabel) {
            statusBar()->showMessage(stopPresentation.statusLabelMessage, 2000);
        }
        return;
    }

    therionRunnerService_->stop();
    if (stopPresentation.shouldUpdateStatusLabel) {
        statusBar()->showMessage(stopPresentation.statusLabelMessage, 2000);
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
    setCompilerStatusResult(exitStatus == QProcess::NormalExit && exitCode == 0,
                            eventPresentation.statusText);
    activeTherionRunConfigPath_.clear();
    updateTherionRunnerState();
}

void MainWindow::handleTherionRunnerError(const QString &errorText)
{
    const TherionStudio::TherionRunnerLifecyclePresenter::EventPresentation eventPresentation =
        TherionStudio::TherionRunnerLifecyclePresenter::presentError(errorText);
    setCompilerStatusResult(false, eventPresentation.statusText);
    activeTherionRunConfigPath_.clear();
    updateTherionRunnerState();
}

void MainWindow::handleTherionRunnerStateChanged(bool running)
{
    if (running && statusCompilerButton_ != nullptr) {
        setCompilerStatusRunning(activeTherionRunConfigPath_);
    }
    updateTherionRunnerState();
}
