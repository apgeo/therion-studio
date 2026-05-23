#include "MainWindowTherionConsoleBuilder.h"

#include <QComboBox>
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

void styleHelperValue(QLabel *label)
{
    if (label == nullptr) {
        return;
    }

    label->setStyleSheet(QStringLiteral(
        "QLabel {"
        " color: palette(mid);"
        " font-size: 11px;"
        "}"));
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

    result.therionArgumentsEdit = new QLineEdit(widget);
    result.therionArgumentsEdit->setPlaceholderText(
        translate("Additional Therion command-line options"));
    result.therionArgumentsEdit->setText(input.persistedArguments);
    result.therionArgumentsEdit->setCursorPosition(result.therionArgumentsEdit->text().size());
    addLabeledWidget(settingsLayout, translate("Arguments"), result.therionArgumentsEdit, widget);

    result.therionRunTargetCombo = new QComboBox(widget);
    result.therionRunTargetCombo->addItem(translate("Current Config"), QStringLiteral("current"));
    result.therionRunTargetCombo->addItem(translate("Project Config"), QStringLiteral("project"));
    const int selectedTargetIndex = result.therionRunTargetCombo->findData(input.persistedRunTargetMode);
    const int defaultTargetIndex = result.therionRunTargetCombo->findData(QStringLiteral("project"));
    result.therionRunTargetCombo->setCurrentIndex(selectedTargetIndex >= 0 ? selectedTargetIndex : defaultTargetIndex);
    addLabeledWidget(settingsLayout, translate("Run Target"), result.therionRunTargetCombo, widget);

    result.therionTargetConfigEdit = new QLineEdit(widget);
    result.therionTargetConfigEdit->setPlaceholderText(translate("Path to project .thconfig"));
    result.therionTargetConfigEdit->setText(input.persistedTargetConfigPath);
    result.therionTargetConfigEdit->setCursorPosition(result.therionTargetConfigEdit->text().size());
    result.therionBrowseTargetConfigButton = new QPushButton(translate("..."), widget);
    result.therionBrowseTargetConfigButton->setToolTip(translate("Browse for target .thconfig"));
    result.therionBrowseTargetConfigButton->setAccessibleName(translate("Browse for target .thconfig"));
    result.therionBrowseTargetConfigButton->setFixedWidth(34);

    auto *targetConfigRow = new QWidget(widget);
    auto *targetConfigRowLayout = new QHBoxLayout(targetConfigRow);
    targetConfigRowLayout->setContentsMargins(0, 0, 0, 0);
    targetConfigRowLayout->setSpacing(6);
    targetConfigRowLayout->addWidget(result.therionTargetConfigEdit, 1);
    targetConfigRowLayout->addWidget(result.therionBrowseTargetConfigButton);
    addLabeledWidget(settingsLayout, translate("Target Config"), targetConfigRow, widget);

    result.therionConfigNameValue = new QLabel(translate("Auto-detect"), widget);
    result.therionConfigNameValue->hide();
    result.therionConfigPathValue =
        new QLabel(translate("No config file resolved from the current context"), widget);
    result.therionConfigPathValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionConfigPathValue->setWordWrap(true);
    result.therionConfigPathValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    styleHelperValue(result.therionConfigPathValue);
    settingsLayout->addWidget(result.therionConfigPathValue);

    result.therionWorkingDirectoryEdit = new QLineEdit(widget);
    result.therionWorkingDirectoryEdit->setPlaceholderText(
        translate("Auto: selected config folder"));
    result.therionWorkingDirectoryEdit->setText(input.persistedWorkingDirectory);
    result.therionWorkingDirectoryEdit->setCursorPosition(result.therionWorkingDirectoryEdit->text().size());
    result.therionBrowseWorkingDirectoryButton = new QPushButton(translate("..."), widget);
    result.therionBrowseWorkingDirectoryButton->setToolTip(
        translate("Browse for working directory override"));
    result.therionBrowseWorkingDirectoryButton->setAccessibleName(
        translate("Browse for working directory override"));
    result.therionBrowseWorkingDirectoryButton->setFixedWidth(34);

    auto *workingDirectoryRow = new QWidget(widget);
    auto *workingDirectoryRowLayout = new QHBoxLayout(workingDirectoryRow);
    workingDirectoryRowLayout->setContentsMargins(0, 0, 0, 0);
    workingDirectoryRowLayout->setSpacing(6);
    workingDirectoryRowLayout->addWidget(result.therionWorkingDirectoryEdit, 1);
    workingDirectoryRowLayout->addWidget(result.therionBrowseWorkingDirectoryButton);
    addLabeledWidget(settingsLayout, translate("Working Directory Override"), workingDirectoryRow, widget);

    result.therionWorkingDirectoryValue =
        new QLabel(translate("Project root or selected config folder"), widget);
    result.therionWorkingDirectoryValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
    result.therionWorkingDirectoryValue->setWordWrap(true);
    result.therionWorkingDirectoryValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    styleHelperValue(result.therionWorkingDirectoryValue);
    settingsLayout->addWidget(result.therionWorkingDirectoryValue);

    result.therionResetWorkingDirectoryButton =
        new QPushButton(translate("Reset Override"), widget);
    result.therionResetWorkingDirectoryButton->setToolTip(
        translate("Clear the project working directory override and use the selected config folder"));
    result.therionResetWorkingDirectoryButton->setEnabled(false);
    result.therionResetWorkingDirectoryButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    settingsLayout->addWidget(result.therionResetWorkingDirectoryButton);
    settingsLayout->addSpacing(10);

    layout->addLayout(settingsLayout);

    auto *buttonGrid = new QGridLayout;
    buttonGrid->setContentsMargins(0, 0, 0, 0);
    buttonGrid->setHorizontalSpacing(6);
    buttonGrid->setVerticalSpacing(6);
    result.therionRunButton = new QPushButton(translate("Run Therion"), widget);
    result.therionStopButton = new QPushButton(translate("Stop"), widget);
    result.therionStopButton->setEnabled(false);
    result.therionClearOutputButton = new QPushButton(translate("Clear Output"), widget);
    result.therionCopyOutputButton = new QPushButton(translate("Copy Output"), widget);

    for (QPushButton *button : {result.therionRunButton,
                                result.therionStopButton,
                                result.therionClearOutputButton,
                                result.therionCopyOutputButton}) {
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    buttonGrid->addWidget(result.therionRunButton, 0, 0);
    buttonGrid->addWidget(result.therionStopButton, 0, 1);
    buttonGrid->addWidget(result.therionClearOutputButton, 1, 0);
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
