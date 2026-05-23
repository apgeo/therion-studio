#include "MainWindowTherionConsoleBuilder.h"

#include <QCoreApplication>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QString translate(const char *sourceText)
{
    return QCoreApplication::translate("MainWindow", sourceText);
}

QLabel *createFieldLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return label;
}

void addLabeledWidget(QVBoxLayout *layout, const QString &labelText, QWidget *widget, QWidget *parent)
{
    layout->addWidget(createFieldLabel(labelText, parent));
    layout->addWidget(widget);
}
}

namespace TherionStudio
{
MainWindowTherionConsoleBuilder::BuildResult
MainWindowTherionConsoleBuilder::build(const BuildInput &input)
{
    BuildResult result;

    auto *widget = new QWidget(input.consoleHost);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *settingsLayout = new QVBoxLayout;
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(5);

    result.therionExecutableEdit = new QLineEdit(widget);
    result.therionExecutableEdit->setPlaceholderText(translate("therion"));
    result.therionExecutableEdit->setText(input.persistedExecutablePath.isEmpty()
                                              ? (input.suggestedExecutablePath.isEmpty()
                                                     ? QStringLiteral("therion")
                                                     : input.suggestedExecutablePath)
                                              : input.persistedExecutablePath);
    result.therionExecutableEdit->setCursorPosition(result.therionExecutableEdit->text().size());
    result.therionBrowseExecutableButton = new QPushButton(translate("..."), widget);
    result.therionBrowseExecutableButton->setToolTip(translate("Browse for Therion executable"));
    result.therionBrowseExecutableButton->setAccessibleName(translate("Browse for Therion executable"));
    result.therionBrowseExecutableButton->setFixedWidth(34);

    auto *executableRow = new QWidget(widget);
    auto *executableRowLayout = new QHBoxLayout(executableRow);
    executableRowLayout->setContentsMargins(0, 0, 0, 0);
    executableRowLayout->setSpacing(6);
    executableRowLayout->addWidget(result.therionExecutableEdit, 1);
    executableRowLayout->addWidget(result.therionBrowseExecutableButton);
    addLabeledWidget(settingsLayout, translate("Executable"), executableRow, widget);

    result.therionWorkingDirectoryEdit = new QLineEdit(widget);
    result.therionWorkingDirectoryEdit->setPlaceholderText(
        translate("Defaults to the current project root"));
    result.therionWorkingDirectoryEdit->setText(input.persistedWorkingDirectory);
    result.therionWorkingDirectoryEdit->setCursorPosition(result.therionWorkingDirectoryEdit->text().size());
    addLabeledWidget(settingsLayout, translate("Working Directory"), result.therionWorkingDirectoryEdit, widget);

    result.therionArgumentsEdit = new QLineEdit(widget);
    result.therionArgumentsEdit->setPlaceholderText(
        translate("Additional Therion command-line options"));
    result.therionArgumentsEdit->setText(input.persistedArguments);
    result.therionArgumentsEdit->setCursorPosition(result.therionArgumentsEdit->text().size());
    addLabeledWidget(settingsLayout, translate("Arguments"), result.therionArgumentsEdit, widget);
    layout->addLayout(settingsLayout);

    result.therionConfigNameValue = new QLabel(translate("Auto-detect"), widget);
    result.therionConfigNameValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionConfigNameValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    result.therionConfigPathValue =
        new QLabel(translate("No config file resolved from the current context"), widget);
    result.therionConfigPathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionConfigPathValue->setWordWrap(true);
    result.therionConfigPathValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    result.therionRunPolicyLabel =
        new QLabel(translate("Reject parallel runs while a Therion process is active"), widget);
    result.therionRunPolicyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionRunPolicyLabel->setWordWrap(true);
    result.therionRunPolicyLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    auto *summaryLayout = new QVBoxLayout;
    summaryLayout->setContentsMargins(0, 4, 0, 0);
    summaryLayout->setSpacing(4);
    addLabeledWidget(summaryLayout, translate("Config"), result.therionConfigNameValue, widget);
    addLabeledWidget(summaryLayout, translate("Config Path"), result.therionConfigPathValue, widget);
    addLabeledWidget(summaryLayout, translate("Run Policy"), result.therionRunPolicyLabel, widget);
    layout->addLayout(summaryLayout);

    result.therionStatusLabel = new QLabel(translate("Idle"), widget);
    result.therionStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionStatusLabel->setWordWrap(true);
    result.therionStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(result.therionStatusLabel);

    auto *buttonGrid = new QGridLayout;
    buttonGrid->setContentsMargins(0, 0, 0, 0);
    buttonGrid->setHorizontalSpacing(6);
    buttonGrid->setVerticalSpacing(6);
    result.therionRunButton = new QPushButton(translate("Run Therion"), widget);
    result.therionStopButton = new QPushButton(translate("Stop"), widget);
    result.therionStopButton->setEnabled(false);
    result.therionResetWorkingDirectoryButton =
        new QPushButton(translate("Use Root"), widget);
    result.therionResetWorkingDirectoryButton->setToolTip(translate("Use project root as working directory"));
    result.therionCopyOutputButton = new QPushButton(translate("Copy Output"), widget);

    for (QPushButton *button : {result.therionRunButton,
                                result.therionStopButton,
                                result.therionResetWorkingDirectoryButton,
                                result.therionCopyOutputButton}) {
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    buttonGrid->addWidget(result.therionRunButton, 0, 0);
    buttonGrid->addWidget(result.therionStopButton, 0, 1);
    buttonGrid->addWidget(result.therionResetWorkingDirectoryButton, 1, 0);
    buttonGrid->addWidget(result.therionCopyOutputButton, 1, 1);
    buttonGrid->setColumnStretch(0, 1);
    buttonGrid->setColumnStretch(1, 1);
    layout->addLayout(buttonGrid);

    result.consoleView = new QPlainTextEdit(widget);
    result.consoleView->setReadOnly(true);
    result.consoleView->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    result.consoleView->setPlaceholderText(translate("Therion runner output will appear here."));
    layout->addWidget(result.consoleView, 1);

    result.rootWidget = widget;
    return result;
}
}
