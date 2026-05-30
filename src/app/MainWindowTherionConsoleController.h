#pragma once

#include <QClipboard>
#include <QGuiApplication>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>

namespace TherionStudio
{
class MainWindowTherionConsoleController final
{
public:
    enum class CopyOutputResult
    {
        Copied,
        Empty,
        ClipboardUnavailable,
        ConsoleUnavailable
    };

    void bindWidgets(QPlainTextEdit *consoleView,
                     QPushButton *runButton,
                     QPushButton *stopButton,
                     QLineEdit *workingDirectoryEdit,
                     QLineEdit *argumentsEdit)
    {
        consoleView_ = consoleView;
        runButton_ = runButton;
        stopButton_ = stopButton;
        workingDirectoryEdit_ = workingDirectoryEdit;
        argumentsEdit_ = argumentsEdit;
    }

    void appendConsoleLine(const QString &line) const
    {
        if (consoleView_ == nullptr) {
            return;
        }
        consoleView_->appendPlainText(line);
    }

    void appendProcessStandardOutput(const QString &output) const
    {
        if (consoleView_ == nullptr || output.isEmpty()) {
            return;
        }
        consoleView_->appendPlainText(output);
    }

    void appendProcessStandardError(const QString &output, const QString &prefix) const
    {
        if (consoleView_ == nullptr || output.isEmpty()) {
            return;
        }
        consoleView_->appendPlainText(prefix.arg(output));
    }

    CopyOutputResult copyConsoleOutputToClipboard() const
    {
        if (consoleView_ == nullptr) {
            return CopyOutputResult::ConsoleUnavailable;
        }

        const QString consoleOutput = consoleView_->toPlainText();
        if (consoleOutput.trimmed().isEmpty()) {
            return CopyOutputResult::Empty;
        }

        QClipboard *clipboard = QGuiApplication::clipboard();
        if (clipboard == nullptr) {
            return CopyOutputResult::ClipboardUnavailable;
        }

        clipboard->setText(consoleOutput);
        return CopyOutputResult::Copied;
    }

    bool clearConsoleOutput() const
    {
        if (consoleView_ == nullptr) {
            return false;
        }

        consoleView_->clear();
        return true;
    }

    void applyRunnerState(bool isRunning) const
    {
        if (runButton_ != nullptr) {
            runButton_->setEnabled(!isRunning);
        }
        if (stopButton_ != nullptr) {
            stopButton_->setEnabled(isRunning);
        }
        if (workingDirectoryEdit_ != nullptr) {
            workingDirectoryEdit_->setEnabled(!isRunning);
        }
        if (argumentsEdit_ != nullptr) {
            argumentsEdit_->setEnabled(!isRunning);
        }
    }

private:
    QPlainTextEdit *consoleView_ = nullptr;
    QPushButton *runButton_ = nullptr;
    QPushButton *stopButton_ = nullptr;
    QLineEdit *workingDirectoryEdit_ = nullptr;
    QLineEdit *argumentsEdit_ = nullptr;
};
}
