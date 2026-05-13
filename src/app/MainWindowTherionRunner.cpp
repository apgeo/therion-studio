#include "MainWindow.h"

#include "MainWindowDocumentHelpers.h"
#include "../core/SessionStore.h"

#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSizePolicy>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QString detectHomebrewTherionExecutablePath()
{
    QStringList candidates;

    const QString brewPrefix = qEnvironmentVariable("HOMEBREW_PREFIX").trimmed();
    if (!brewPrefix.isEmpty()) {
        candidates.append(QDir(brewPrefix).absoluteFilePath(QStringLiteral("bin/therion")));
    }

    candidates.append(QStringLiteral("/opt/homebrew/bin/therion"));
    candidates.append(QStringLiteral("/usr/local/bin/therion"));

    for (const QString &candidatePath : candidates) {
        const QFileInfo candidateInfo(candidatePath);
        if (!candidateInfo.exists() || !candidateInfo.isFile() || !candidateInfo.isExecutable()) {
            continue;
        }

        const QString canonicalPath = candidateInfo.canonicalFilePath();
        return canonicalPath.isEmpty() ? candidateInfo.absoluteFilePath() : canonicalPath;
    }

    return QString();
}
}

void MainWindow::buildConsole()
{
    QWidget *consoleHost = consoleSidebarPage_ != nullptr ? consoleSidebarPage_ : this;
    auto *widget = new QWidget(consoleHost);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    therionExecutableEdit_ = new QLineEdit(widget);
    therionExecutableEdit_->setPlaceholderText(tr("therion"));
    const QString persistedExecutablePath = TherionStudio::SessionStore::therionExecutablePath().trimmed();
    const QString detectedHomebrewPath = detectHomebrewTherionExecutablePath();
    therionExecutableEdit_->setText(persistedExecutablePath.isEmpty()
                                       ? (detectedHomebrewPath.isEmpty() ? QStringLiteral("therion") : detectedHomebrewPath)
                                       : persistedExecutablePath);
    therionBrowseExecutableButton_ = new QPushButton(tr("Browse..."), widget);

    auto *executableRow = new QWidget(widget);
    auto *executableRowLayout = new QHBoxLayout(executableRow);
    executableRowLayout->setContentsMargins(0, 0, 0, 0);
    executableRowLayout->setSpacing(6);
    executableRowLayout->addWidget(therionExecutableEdit_, 1);
    executableRowLayout->addWidget(therionBrowseExecutableButton_);

    therionWorkingDirectoryEdit_ = new QLineEdit(widget);
    therionWorkingDirectoryEdit_->setPlaceholderText(tr("Defaults to the current project root"));
    therionWorkingDirectoryEdit_->setText(TherionStudio::SessionStore::therionWorkingDirectory().trimmed());

    therionArgumentsEdit_ = new QLineEdit(widget);
    therionArgumentsEdit_->setPlaceholderText(tr("Additional Therion command-line options"));
    therionArgumentsEdit_->setText(TherionStudio::SessionStore::therionArguments().trimmed());

    therionConfigNameValue_ = new QLabel(tr("Auto-detect"), widget);
    therionConfigNameValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    therionConfigPathValue_ = new QLabel(tr("No config file resolved from the current context"), widget);
    therionConfigPathValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    therionConfigPathValue_->setWordWrap(true);
    therionConfigPathValue_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    therionRunPolicyLabel_ = new QLabel(tr("Reject parallel runs while a Therion process is active"), widget);
    therionRunPolicyLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    therionRunPolicyLabel_->setWordWrap(true);
    therionRunPolicyLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    form->addRow(tr("Executable"), executableRow);
    form->addRow(tr("Working Directory"), therionWorkingDirectoryEdit_);
    form->addRow(tr("Arguments"), therionArgumentsEdit_);
    form->addRow(tr("Config"), therionConfigNameValue_);
    form->addRow(tr("Config Path"), therionConfigPathValue_);
    form->addRow(tr("Run Policy"), therionRunPolicyLabel_);
    layout->addLayout(form);

    therionStatusLabel_ = new QLabel(tr("Idle"), widget);
    therionStatusLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    therionStatusLabel_->setWordWrap(true);
    therionStatusLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(therionStatusLabel_);

    auto *buttonRow = new QHBoxLayout;
    therionRunButton_ = new QPushButton(tr("Run Therion"), widget);
    therionStopButton_ = new QPushButton(tr("Stop"), widget);
    therionStopButton_->setEnabled(false);
    therionResetWorkingDirectoryButton_ = new QPushButton(tr("Use Project Root"), widget);
    therionCopyOutputButton_ = new QPushButton(tr("Copy Output"), widget);

    connect(therionRunButton_, &QPushButton::clicked, this, &MainWindow::runTherion);
    connect(therionStopButton_, &QPushButton::clicked, this, &MainWindow::stopTherion);
    connect(therionBrowseExecutableButton_, &QPushButton::clicked, this, &MainWindow::browseTherionExecutable);
    connect(therionResetWorkingDirectoryButton_, &QPushButton::clicked, this, [this]() {
        therionWorkingDirectoryEdit_->setText(projectRootPath_);
    });
    connect(therionCopyOutputButton_, &QPushButton::clicked, this, &MainWindow::copyTherionConsoleOutput);
    connect(therionWorkingDirectoryEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        refreshTherionConfigDisplay();
    });
    connect(therionArgumentsEdit_, &QLineEdit::textChanged, this, [this](const QString &) {
        refreshTherionConfigDisplay();
    });

    buttonRow->addWidget(therionRunButton_);
    buttonRow->addWidget(therionStopButton_);
    buttonRow->addWidget(therionResetWorkingDirectoryButton_);
    buttonRow->addWidget(therionCopyOutputButton_);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    consoleView_ = new QPlainTextEdit(widget);
    consoleView_->setReadOnly(true);
    consoleView_->setPlaceholderText(tr("Therion runner output will appear here."));
    layout->addWidget(consoleView_, 1);

    therionProcess_ = new QProcess(this);
    connect(therionProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::handleTherionProcessReadyReadStandardOutput);
    connect(therionProcess_, &QProcess::readyReadStandardError, this, &MainWindow::handleTherionProcessReadyReadStandardError);
    connect(therionProcess_, &QProcess::finished, this, &MainWindow::handleTherionProcessFinished);
    connect(therionProcess_, &QProcess::errorOccurred, this, &MainWindow::handleTherionProcessError);
    if (consoleSidebarPageLayout_ != nullptr) {
        consoleSidebarPageLayout_->addWidget(widget, 1);
    }
    appendConsoleLine(tr("Therion Studio shell initialized."));
    appendConsoleLine(tr("Run policy: reject parallel runs while a Therion process is active."));
    refreshTherionConfigDisplay();
    updateTherionRunnerState();
}

void MainWindow::appendConsoleLine(const QString &line)
{
    if (consoleView_ == nullptr) {
        return;
    }

    consoleView_->appendPlainText(line);
}

void MainWindow::copyTherionConsoleOutput()
{
    if (consoleView_ == nullptr) {
        return;
    }

    const QString consoleOutput = consoleView_->toPlainText();
    if (consoleOutput.trimmed().isEmpty()) {
        statusBar()->showMessage(tr("Console output is empty"), 2000);
        return;
    }

    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        statusBar()->showMessage(tr("Clipboard is unavailable"), 2000);
        return;
    }

    clipboard->setText(consoleOutput);
    statusBar()->showMessage(tr("Copied console output"), 2000);
}

void MainWindow::browseTherionExecutable()
{
    QString initialPath = therionExecutableEdit_ != nullptr ? therionExecutableEdit_->text().trimmed() : QString();
    QFileInfo initialInfo(initialPath);
    if (initialPath.isEmpty() || !initialInfo.exists()) {
        initialPath = QDir::homePath();
    } else if (initialInfo.isFile()) {
        initialPath = initialInfo.absolutePath();
    }

    const QString selectedExecutablePath = QFileDialog::getOpenFileName(this,
                                                                         tr("Select Therion Executable"),
                                                                         initialPath);
    if (selectedExecutablePath.isEmpty()) {
        return;
    }

    QFileInfo selectedInfo(selectedExecutablePath);
    if (!selectedInfo.isExecutable()) {
        QMessageBox::warning(this,
                             tr("Select Therion Executable"),
                             tr("The selected file is not executable."));
        return;
    }

    if (therionExecutableEdit_ != nullptr) {
        therionExecutableEdit_->setText(selectedInfo.absoluteFilePath());
    }

    statusBar()->showMessage(tr("Therion executable updated"), 2000);
}

QString MainWindow::resolvedTherionWorkingDirectory() const
{
    const QString typedWorkingDirectory = therionWorkingDirectoryEdit_ != nullptr ? therionWorkingDirectoryEdit_->text().trimmed() : QString();
    if (!typedWorkingDirectory.isEmpty()) {
        return QDir(typedWorkingDirectory).absolutePath();
    }

    if (!projectRootPath_.isEmpty()) {
        return QDir(projectRootPath_).absolutePath();
    }

    return QString();
}

bool MainWindow::hasExplicitTherionConfigArgument() const
{
    const QStringList arguments = QProcess::splitCommand(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString());
    for (int index = 0; index < arguments.size(); ++index) {
        const QString token = arguments.at(index).trimmed();
        if (token == QStringLiteral("-c") || token == QStringLiteral("--config")) {
            return true;
        }
        if (token.startsWith(QStringLiteral("--config=")) || token.startsWith(QStringLiteral("-c="))) {
            return true;
        }
    }

    return false;
}

QString MainWindow::resolvedTherionConfigPath() const
{
    auto resolveCandidatePath = [this](const QString &candidate) -> QString {
        const QString trimmedCandidate = candidate.trimmed();
        if (trimmedCandidate.isEmpty()) {
            return QString();
        }

        QFileInfo candidateInfo(trimmedCandidate);
        QString absolutePath;
        if (candidateInfo.isAbsolute()) {
            absolutePath = candidateInfo.absoluteFilePath();
        } else {
            const QString baseDirectory = resolvedTherionWorkingDirectory().isEmpty() ? projectRootPath_ : resolvedTherionWorkingDirectory();
            if (baseDirectory.isEmpty()) {
                return QString();
            }
            absolutePath = QDir(baseDirectory).absoluteFilePath(trimmedCandidate);
        }

        QFileInfo resolvedInfo(absolutePath);
        if (!resolvedInfo.exists()) {
            return QString();
        }

        const QString canonicalPath = resolvedInfo.canonicalFilePath();
        return canonicalPath.isEmpty() ? resolvedInfo.absoluteFilePath() : canonicalPath;
    };

    const QStringList arguments = QProcess::splitCommand(therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString());
    for (int index = 0; index < arguments.size(); ++index) {
        const QString token = arguments.at(index).trimmed();
        if (token == QStringLiteral("-c") || token == QStringLiteral("--config")) {
            if (index + 1 < arguments.size()) {
                const QString resolvedPath = resolveCandidatePath(arguments.at(index + 1));
                if (!resolvedPath.isEmpty()) {
                    return resolvedPath;
                }
            }
            continue;
        }

        if (token.startsWith(QStringLiteral("--config="))) {
            const QString resolvedPath = resolveCandidatePath(token.mid(QStringLiteral("--config=").size()));
            if (!resolvedPath.isEmpty()) {
                return resolvedPath;
            }
            continue;
        }

        if (token.startsWith(QStringLiteral("-c="))) {
            const QString resolvedPath = resolveCandidatePath(token.mid(QStringLiteral("-c=").size()));
            if (!resolvedPath.isEmpty()) {
                return resolvedPath;
            }
        }
    }

    QWidget *activeWidget = currentDocumentWidget();
    const QString activeDocumentPath = activeWidget != nullptr ? documentPathForWidget(activeWidget) : QString();
    if (activeDocumentPath.endsWith(QStringLiteral(".thconfig"), Qt::CaseInsensitive)) {
        QFileInfo activeDocumentInfo(activeDocumentPath);
        if (activeDocumentInfo.exists()) {
            const QString canonicalPath = activeDocumentInfo.canonicalFilePath();
            return canonicalPath.isEmpty() ? activeDocumentInfo.absoluteFilePath() : canonicalPath;
        }
    }

    const QString workingDirectory = resolvedTherionWorkingDirectory();
    const QString baseDirectory = workingDirectory.isEmpty() ? projectRootPath_ : workingDirectory;
    if (baseDirectory.isEmpty()) {
        return QString();
    }

    const QString defaultConfigPath = QDir(baseDirectory).absoluteFilePath(QStringLiteral("thconfig"));
    QFileInfo defaultConfigInfo(defaultConfigPath);
    if (!defaultConfigInfo.exists()) {
        return QString();
    }

    const QString canonicalPath = defaultConfigInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? defaultConfigInfo.absoluteFilePath() : canonicalPath;
}

void MainWindow::refreshTherionConfigDisplay()
{
    if (therionConfigNameValue_ == nullptr || therionConfigPathValue_ == nullptr) {
        return;
    }

    const bool explicitConfigArgument = hasExplicitTherionConfigArgument();
    const QString resolvedPath = resolvedTherionConfigPath();
    if (resolvedPath.isEmpty()) {
        therionConfigNameValue_->setText(tr("Auto-detect"));
        therionConfigPathValue_->setText(tr("No config file resolved from the current context"));
        therionConfigPathValue_->setToolTip(QString());
        autoResolvedTherionWorkingDirectory_.clear();
        return;
    }

    const QFileInfo configInfo(resolvedPath);
    therionConfigNameValue_->setText(configInfo.fileName().isEmpty() ? tr("thconfig") : configInfo.fileName());
    therionConfigPathValue_->setText(resolvedPath);
    therionConfigPathValue_->setToolTip(resolvedPath);

    if (!explicitConfigArgument && therionWorkingDirectoryEdit_ != nullptr) {
        const QString configDirectory = configInfo.absolutePath();
        const QString normalizedCurrentWorkingDirectory = QDir(therionWorkingDirectoryEdit_->text().trimmed()).absolutePath();
        const QString normalizedProjectRoot = projectRootPath_.isEmpty() ? QString() : QDir(projectRootPath_).absolutePath();
        const QString normalizedAutoResolvedWorkingDirectory = autoResolvedTherionWorkingDirectory_.isEmpty()
            ? QString()
            : QDir(autoResolvedTherionWorkingDirectory_).absolutePath();

        const bool shouldAutoUpdateWorkingDirectory = therionWorkingDirectoryEdit_->text().trimmed().isEmpty()
            || (!normalizedProjectRoot.isEmpty() && normalizedCurrentWorkingDirectory == normalizedProjectRoot)
            || (!normalizedAutoResolvedWorkingDirectory.isEmpty() && normalizedCurrentWorkingDirectory == normalizedAutoResolvedWorkingDirectory);

        if (shouldAutoUpdateWorkingDirectory && normalizedCurrentWorkingDirectory != configDirectory) {
            therionWorkingDirectoryEdit_->setText(configDirectory);
        }

        autoResolvedTherionWorkingDirectory_ = configDirectory;
    }
}

void MainWindow::updateTherionRunnerState()
{
    const bool isRunning = therionProcess_ != nullptr && therionProcess_->state() != QProcess::NotRunning;
    if (therionRunButton_ != nullptr) {
        therionRunButton_->setEnabled(!isRunning);
    }
    if (therionStopButton_ != nullptr) {
        therionStopButton_->setEnabled(isRunning);
    }
    if (therionExecutableEdit_ != nullptr) {
        therionExecutableEdit_->setEnabled(!isRunning);
    }
    if (therionBrowseExecutableButton_ != nullptr) {
        therionBrowseExecutableButton_->setEnabled(!isRunning);
    }
    if (therionWorkingDirectoryEdit_ != nullptr) {
        therionWorkingDirectoryEdit_->setEnabled(!isRunning);
    }
    if (therionArgumentsEdit_ != nullptr) {
        therionArgumentsEdit_->setEnabled(!isRunning);
    }
}

void MainWindow::runTherion()
{
    if (therionProcess_ == nullptr) {
        appendConsoleLine(tr("Therion runner is unavailable."));
        return;
    }

    if (therionProcess_->state() != QProcess::NotRunning) {
        const QString alreadyRunningMessage = tr("Therion is already running. Parallel runs are rejected until the current run finishes.");
        appendConsoleLine(alreadyRunningMessage);
        statusBar()->showMessage(alreadyRunningMessage, 3000);
        if (therionStatusLabel_ != nullptr) {
            therionStatusLabel_->setText(alreadyRunningMessage);
        }
        return;
    }

    const QString workingDirectory = resolvedTherionWorkingDirectory();
    if (workingDirectory.isEmpty()) {
        QMessageBox::warning(this,
                             tr("Run Therion"),
                             tr("Open a project or set a working directory before running Therion."));
        return;
    }

    if (!QDir(workingDirectory).exists()) {
        QMessageBox::warning(this,
                             tr("Run Therion"),
                             tr("The selected working directory does not exist."));
        return;
    }

    const QString executableInput = therionExecutableEdit_ != nullptr && !therionExecutableEdit_->text().trimmed().isEmpty()
                                        ? therionExecutableEdit_->text().trimmed()
                                        : QStringLiteral("therion");
    QString resolvedExecutablePath;
    const QFileInfo executableInfo(executableInput);
    if (executableInfo.isAbsolute()) {
        resolvedExecutablePath = executableInfo.absoluteFilePath();
    } else if (executableInput.contains(QLatin1Char('/')) || executableInput.contains(QLatin1Char('\\'))) {
        resolvedExecutablePath = QDir(workingDirectory).absoluteFilePath(executableInput);
    } else {
        resolvedExecutablePath = QStandardPaths::findExecutable(executableInput);
        if (resolvedExecutablePath.isEmpty()) {
            resolvedExecutablePath = detectHomebrewTherionExecutablePath();
            if (!resolvedExecutablePath.isEmpty() && therionExecutableEdit_ != nullptr && executableInput == QStringLiteral("therion")) {
                therionExecutableEdit_->setText(resolvedExecutablePath);
            }
        }
    }

    const QFileInfo resolvedExecutableInfo(resolvedExecutablePath);
    if (resolvedExecutablePath.isEmpty() || !resolvedExecutableInfo.exists() || !resolvedExecutableInfo.isFile() || !resolvedExecutableInfo.isExecutable()) {
        const QString message = tr("Therion executable \"%1\" was not found or is not executable. Set Executable to a full path (for example `/opt/homebrew/bin/therion`) or install Therion in the application PATH.")
                                    .arg(executableInput);
        QMessageBox::warning(this, tr("Run Therion"), message);
        appendConsoleLine(message);
        if (therionStatusLabel_ != nullptr) {
            therionStatusLabel_->setText(message);
        }
        return;
    }

    const QString argumentsText = therionArgumentsEdit_ != nullptr ? therionArgumentsEdit_->text().trimmed() : QString();
    const QStringList arguments = argumentsText.isEmpty() ? QStringList() : QProcess::splitCommand(argumentsText);

    therionProcess_->setWorkingDirectory(workingDirectory);
    therionProcess_->start(resolvedExecutablePath, arguments);

    appendConsoleLine(tr("Running %1 %2 in %3")
                          .arg(resolvedExecutablePath)
                          .arg(argumentsText)
                          .arg(workingDirectory));
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(tr("Starting Therion..."));
    }
    updateTherionRunnerState();
}

void MainWindow::stopTherion()
{
    if (therionProcess_ == nullptr || therionProcess_->state() == QProcess::NotRunning) {
        appendConsoleLine(tr("Therion is not running."));
        return;
    }

    appendConsoleLine(tr("Stopping Therion runner..."));
    therionProcess_->kill();
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(tr("Stopping Therion..."));
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionProcessReadyReadStandardOutput()
{
    if (therionProcess_ == nullptr || consoleView_ == nullptr) {
        return;
    }

    const QString output = QString::fromLocal8Bit(therionProcess_->readAllStandardOutput());
    if (!output.isEmpty()) {
        consoleView_->appendPlainText(output);
    }
}

void MainWindow::handleTherionProcessReadyReadStandardError()
{
    if (therionProcess_ == nullptr || consoleView_ == nullptr) {
        return;
    }

    const QString output = QString::fromLocal8Bit(therionProcess_->readAllStandardError());
    if (!output.isEmpty()) {
        consoleView_->appendPlainText(tr("[stderr] %1").arg(output));
    }
}

void MainWindow::handleTherionProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    const QString statusText = exitStatus == QProcess::NormalExit
        ? tr("Therion finished with exit code %1.").arg(exitCode)
        : tr("Therion crashed while running.");
    appendConsoleLine(statusText);
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(statusText);
    }
    updateTherionRunnerState();
}

void MainWindow::handleTherionProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    if (therionProcess_ == nullptr) {
        return;
    }

    const QString errorText = tr("Therion runner error: %1").arg(therionProcess_->errorString());
    appendConsoleLine(errorText);
    if (therionStatusLabel_ != nullptr) {
        therionStatusLabel_->setText(errorText);
    }
    updateTherionRunnerState();
}
