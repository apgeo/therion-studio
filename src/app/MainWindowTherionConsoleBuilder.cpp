#include "MainWindowTherionConsoleBuilder.h"

#include <QCoreApplication>
#include <QFormLayout>
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
}

namespace TherionStudio
{
MainWindowTherionConsoleBuilder::BuildResult
MainWindowTherionConsoleBuilder::build(const BuildInput &input)
{
    BuildResult result;

    auto *widget = new QWidget(input.consoleHost);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFormAlignment(Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);

    result.therionExecutableEdit = new QLineEdit(widget);
    result.therionExecutableEdit->setPlaceholderText(translate("therion"));
    result.therionExecutableEdit->setText(input.persistedExecutablePath.isEmpty()
                                              ? (input.suggestedExecutablePath.isEmpty()
                                                     ? QStringLiteral("therion")
                                                     : input.suggestedExecutablePath)
                                              : input.persistedExecutablePath);
    result.therionBrowseExecutableButton = new QPushButton(translate("Browse..."), widget);

    auto *executableRow = new QWidget(widget);
    auto *executableRowLayout = new QHBoxLayout(executableRow);
    executableRowLayout->setContentsMargins(0, 0, 0, 0);
    executableRowLayout->setSpacing(6);
    executableRowLayout->addWidget(result.therionExecutableEdit, 1);
    executableRowLayout->addWidget(result.therionBrowseExecutableButton);

    result.therionWorkingDirectoryEdit = new QLineEdit(widget);
    result.therionWorkingDirectoryEdit->setPlaceholderText(
        translate("Defaults to the current project root"));
    result.therionWorkingDirectoryEdit->setText(input.persistedWorkingDirectory);

    result.therionArgumentsEdit = new QLineEdit(widget);
    result.therionArgumentsEdit->setPlaceholderText(
        translate("Additional Therion command-line options"));
    result.therionArgumentsEdit->setText(input.persistedArguments);

    result.therionConfigNameValue = new QLabel(translate("Auto-detect"), widget);
    result.therionConfigNameValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
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

    form->addRow(translate("Executable"), executableRow);
    form->addRow(translate("Working Directory"), result.therionWorkingDirectoryEdit);
    form->addRow(translate("Arguments"), result.therionArgumentsEdit);
    form->addRow(translate("Config"), result.therionConfigNameValue);
    form->addRow(translate("Config Path"), result.therionConfigPathValue);
    form->addRow(translate("Run Policy"), result.therionRunPolicyLabel);
    layout->addLayout(form);

    result.therionStatusLabel = new QLabel(translate("Idle"), widget);
    result.therionStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionStatusLabel->setWordWrap(true);
    result.therionStatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(result.therionStatusLabel);

    auto *buttonRow = new QHBoxLayout;
    result.therionRunButton = new QPushButton(translate("Run Therion"), widget);
    result.therionStopButton = new QPushButton(translate("Stop"), widget);
    result.therionStopButton->setEnabled(false);
    result.therionResetWorkingDirectoryButton =
        new QPushButton(translate("Use Project Root"), widget);
    result.therionCopyOutputButton = new QPushButton(translate("Copy Output"), widget);

    buttonRow->addWidget(result.therionRunButton);
    buttonRow->addWidget(result.therionStopButton);
    buttonRow->addWidget(result.therionResetWorkingDirectoryButton);
    buttonRow->addWidget(result.therionCopyOutputButton);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    result.consoleView = new QPlainTextEdit(widget);
    result.consoleView->setReadOnly(true);
    result.consoleView->setPlaceholderText(translate("Therion runner output will appear here."));
    layout->addWidget(result.consoleView, 1);

    result.rootWidget = widget;
    return result;
}
}
