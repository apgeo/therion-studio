#include "BlockEditorCommandOptionsDialog.h"

#include "BlockEditorCommandOptionParser.h"
#include "../ContextHelpController.h"
#include "../TextEditorOptionValidation.h"
#include "../TextEditorTab.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

namespace TherionStudio
{
BlockEditorCommandOptionsDialog::BlockEditorCommandOptionsDialog(TextEditorTab *owner)
    : owner_(owner)
{
}

std::optional<QString> BlockEditorCommandOptionsDialog::configureLine(
    const QString &commandName,
    const QString &sourceLine,
    int lineNumber,
    BlockEditorCommandIdFieldMode idFieldMode)
{
    if (owner_ == nullptr || lineNumber <= 0) {
        return std::nullopt;
    }

    const TherionParsedLine commandParsedLine = TherionDocumentParser::parseLine(sourceLine, lineNumber);
    if (commandParsedLine.tokens.isEmpty()) {
        return std::nullopt;
    }

    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto match = indentPattern.match(sourceLine);
    const QString indent = match.hasMatch() ? match.captured(0) : QString();
    const bool hasIdField = idFieldMode != BlockEditorCommandIdFieldMode::None;
    const bool requiresId = idFieldMode == BlockEditorCommandIdFieldMode::Required;

    const BlockEditorParsedCommandOptions parsedOptions =
        parseBlockEditorCommandOptions(commandName,
                                       commandParsedLine.tokens,
                                       owner_->commandMetadata().commandOptionFixedArityByKey,
                                       hasIdField);
    const QString currentAdditionalPositionalTokens = parsedOptions.extraPositionalTokens.join(QLatin1Char(' '));

    QDialog dialog(owner_);
    dialog.setWindowTitle(TextEditorTab::tr("Configure %1").arg(commandName));
    dialog.setModal(true);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *formLayout = new QFormLayout;
    QLineEdit *idEdit = nullptr;
    if (hasIdField) {
        idEdit = new QLineEdit(parsedOptions.leadingValue, &dialog);
        idEdit->setPlaceholderText(requiresId ? TextEditorTab::tr("required") : TextEditorTab::tr("optional"));
        formLayout->addRow(TextEditorTab::tr("ID"), idEdit);
    }
    QLineEdit *additionalPositionalTokensEdit = nullptr;
    if (!currentAdditionalPositionalTokens.isEmpty()) {
        additionalPositionalTokensEdit = new QLineEdit(currentAdditionalPositionalTokens, &dialog);
        additionalPositionalTokensEdit->setToolTip(
            TextEditorTab::tr("Preserved positional tokens that are not parsed as options. "
                              "Prefer using explicit options whenever possible."));
        formLayout->addRow(TextEditorTab::tr("Extra Arguments (Advanced)"), additionalPositionalTokensEdit);
    }
    layout->addLayout(formLayout);

    auto *optionsLabel = new QLabel(TextEditorTab::tr("Options"), &dialog);
    layout->addWidget(optionsLabel);

    auto *optionsActionsLayout = new QHBoxLayout;
    optionsActionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsActionsLayout->setSpacing(6);
    auto *addOptionButton = new QPushButton(TextEditorTab::tr("Add New Option"), &dialog);
    auto *removeOptionButton = new QPushButton(TextEditorTab::tr("Remove Option"), &dialog);
    addOptionButton->setAutoDefault(false);
    removeOptionButton->setAutoDefault(false);
    optionsActionsLayout->addWidget(addOptionButton);
    optionsActionsLayout->addWidget(removeOptionButton);
    optionsActionsLayout->addStretch(1);
    layout->addLayout(optionsActionsLayout);

    auto *optionsTable = new QTableWidget(&dialog);
    optionsTable->setColumnCount(2);
    optionsTable->setHorizontalHeaderLabels({TextEditorTab::tr("Option"), TextEditorTab::tr("Value")});
    optionsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    optionsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    optionsTable->verticalHeader()->setVisible(false);
    optionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    optionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    optionsTable->setAlternatingRowColors(true);
    optionsTable->setMinimumHeight(160);
    optionsTable->setRowCount(parsedOptions.optionEntries.size());
    for (int row = 0; row < parsedOptions.optionEntries.size(); ++row) {
        const BlockEditorParsedOptionEntry &entry = parsedOptions.optionEntries.at(row);
        optionsTable->setItem(row, 0, new QTableWidgetItem(entry.key));
        optionsTable->setItem(row, 1, new QTableWidgetItem(entry.value));
    }
    layout->addWidget(optionsTable, 1);

    auto *helpLabel = new QLabel(TextEditorTab::tr("Contextual Help"), &dialog);
    QFont helpLabelFont = helpLabel->font();
    helpLabelFont.setBold(true);
    helpLabel->setFont(helpLabelFont);
    layout->addWidget(helpLabel);

    auto *helpBrowser = new QTextBrowser(&dialog);
    helpBrowser->setOpenLinks(false);
    helpBrowser->setOpenExternalLinks(false);
    helpBrowser->setMinimumHeight(160);
    layout->addWidget(helpBrowser, 1);

    const TherionHelpEntry commandHelpEntry = owner_->commandMetadata().helpEntries.value(commandName);
    const QString commandHelpHtml = ContextHelpController::renderHelpHtml(commandName,
                                                                           commandHelpEntry.summary,
                                                                           commandHelpEntry.syntax,
                                                                           commandHelpEntry.arguments,
                                                                           commandHelpEntry.acceptedValues,
                                                                           commandHelpEntry.options,
                                                                           true);

    QString idHelpHtml;
    if (hasIdField) {
        QString idArgumentLine;
        for (const QString &argumentLine : commandHelpEntry.arguments) {
            if (argumentLine.contains(QStringLiteral("<id>"), Qt::CaseInsensitive)) {
                idArgumentLine = argumentLine.trimmed();
                break;
            }
        }
        if (idArgumentLine.isEmpty()) {
            for (const QString &argumentLine : commandHelpEntry.arguments) {
                if (owner_->isRequiredArgumentSignatureForBlocks(argumentLine.section(QLatin1Char('='), 0, 0).trimmed())) {
                    idArgumentLine = argumentLine.trimmed();
                    break;
                }
            }
        }
        if (idArgumentLine.isEmpty() && !commandHelpEntry.arguments.isEmpty()) {
            idArgumentLine = commandHelpEntry.arguments.first().trimmed();
        }

        QString signature = idArgumentLine;
        QString description;
        const int equalsIndex = idArgumentLine.indexOf(QLatin1Char('='));
        if (equalsIndex >= 0) {
            signature = idArgumentLine.left(equalsIndex).trimmed();
            description = idArgumentLine.mid(equalsIndex + 1).trimmed();
        }

        QStringList html;
        html << QStringLiteral("<p><b>Parameter:</b> %1</p>").arg(signature.isEmpty()
                                                                       ? QStringLiteral("&lt;id&gt;")
                                                                       : signature.toHtmlEscaped());
        if (!description.isEmpty()) {
            html << QStringLiteral("<p><b>Description:</b> %1</p>").arg(description.toHtmlEscaped());
        }
        idHelpHtml = html.join(QString());
    }

    const auto updateHelpForCurrentOption = [this, helpBrowser, optionsTable, commandName, commandHelpHtml]() {
        if (helpBrowser == nullptr || optionsTable == nullptr) {
            return;
        }

        const int row = optionsTable->currentRow();
        const QString optionToken = row >= 0 && optionsTable->item(row, 0) != nullptr
            ? optionsTable->item(row, 0)->text().trimmed().toLower()
            : QString();
        const QString optionHelp = owner_->commandMetadata().commandOptionHelpHtmlByKey.value(commandOptionHelpKey(commandName, optionToken));
        if (!optionHelp.trimmed().isEmpty()) {
            helpBrowser->setHtml(optionHelp);
            return;
        }

        helpBrowser->setHtml(commandHelpHtml);
    };
    const auto updateHelpForId = [helpBrowser, idHelpHtml, commandHelpHtml]() {
        if (helpBrowser == nullptr) {
            return;
        }
        helpBrowser->setHtml(!idHelpHtml.isEmpty() ? idHelpHtml : commandHelpHtml);
    };
    const auto updateHelpForAdditionalPositionalTokens = [helpBrowser, commandHelpHtml]() {
        if (helpBrowser == nullptr) {
            return;
        }
        const QString html = QStringLiteral("<p><b>Additional positional tokens</b> keep unsupported tokens intact.</p>"
                                            "<p>Prefer explicit key/value options where available.</p>%1")
                                 .arg(commandHelpHtml);
        helpBrowser->setHtml(html);
    };
    if (hasIdField) {
        updateHelpForId();
    } else {
        updateHelpForCurrentOption();
    }

    QObject::connect(optionsTable, &QTableWidget::currentCellChanged, &dialog, [updateHelpForCurrentOption](int, int, int, int) {
        updateHelpForCurrentOption();
    });
    QObject::connect(optionsTable, &QTableWidget::itemChanged, &dialog, [updateHelpForCurrentOption](QTableWidgetItem *) {
        updateHelpForCurrentOption();
    });
    if (idEdit != nullptr) {
        QObject::connect(idEdit, &QLineEdit::selectionChanged, &dialog, [updateHelpForId]() {
            updateHelpForId();
        });
        QObject::connect(idEdit, &QLineEdit::textEdited, &dialog, [updateHelpForId](const QString &) {
            updateHelpForId();
        });
    }
    if (additionalPositionalTokensEdit != nullptr) {
        QObject::connect(additionalPositionalTokensEdit, &QLineEdit::selectionChanged, &dialog, [updateHelpForAdditionalPositionalTokens]() {
            updateHelpForAdditionalPositionalTokens();
        });
        QObject::connect(additionalPositionalTokensEdit, &QLineEdit::textEdited, &dialog, [updateHelpForAdditionalPositionalTokens](const QString &) {
            updateHelpForAdditionalPositionalTokens();
        });
    }

    QObject::connect(addOptionButton, &QPushButton::clicked, &dialog, [this, optionsTable, commandName, updateHelpForCurrentOption]() {
        if (optionsTable == nullptr) {
            return;
        }
        const int row = optionsTable->rowCount();
        optionsTable->insertRow(row);
        const QString defaultOption = !owner_->commandMetadata().commandOptionTokens.value(commandName).isEmpty()
            ? owner_->commandMetadata().commandOptionTokens.value(commandName).first()
            : QStringLiteral("-option");
        optionsTable->setItem(row, 0, new QTableWidgetItem(defaultOption));
        optionsTable->setItem(row, 1, new QTableWidgetItem(QString()));
        optionsTable->setCurrentCell(row, 0);
        optionsTable->editItem(optionsTable->item(row, 0));
        updateHelpForCurrentOption();
    });

    QObject::connect(removeOptionButton, &QPushButton::clicked, &dialog, [optionsTable, updateHelpForCurrentOption]() {
        if (optionsTable == nullptr || optionsTable->rowCount() == 0) {
            return;
        }
        const int row = optionsTable->currentRow() >= 0 ? optionsTable->currentRow() : optionsTable->rowCount() - 1;
        optionsTable->removeRow(row);
        updateHelpForCurrentOption();
    });
    if (optionsTable->rowCount() > 0) {
        optionsTable->setCurrentCell(0, 0);
    }

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const QString updatedId = hasIdField && idEdit != nullptr ? idEdit->text().trimmed() : QString();
    if (requiresId && updatedId.isEmpty()) {
        QMessageBox::warning(owner_, TextEditorTab::tr("Configure Block"), TextEditorTab::tr("ID cannot be empty."));
        return std::nullopt;
    }

    QVector<TextEditorOptionRow> optionRows;
    optionRows.reserve(optionsTable->rowCount());
    for (int row = 0; row < optionsTable->rowCount(); ++row) {
        const QString key = (optionsTable->item(row, 0) != nullptr
                                 ? optionsTable->item(row, 0)->text()
                                 : QString())
                                .trimmed();
        const QString value = (optionsTable->item(row, 1) != nullptr
                                   ? optionsTable->item(row, 1)->text()
                                   : QString())
                                  .trimmed();
        optionRows.append(TextEditorOptionRow{key, value, row + 1});
    }
    const TextEditorOptionValidationResult validation = validateAndSerializeCommandOptions(
        commandName,
        optionRows,
        owner_->commandMetadata().commandOptionValueArityTokens,
        owner_->commandMetadata().commandOptionFixedArityByKey,
        owner_->commandMetadata().commandOptionArgumentLabelsByKey,
        owner_->commandMetadata().commandOptionValueTokens,
        false);
    if (!validation.ok) {
        if (validation.failingRow >= 0 && validation.failingRow < optionsTable->rowCount()) {
            optionsTable->setCurrentCell(validation.failingRow, 0);
        }
        QMessageBox::warning(owner_, TextEditorTab::tr("Configure Block"), validation.errorMessage);
        return std::nullopt;
    }

    QString updatedLine = QStringLiteral("%1%2").arg(indent, commandName);
    if (hasIdField && !updatedId.isEmpty()) {
        updatedLine += QStringLiteral(" ") + updatedId;
    }
    const QString updatedAdditionalPositionalTokens = additionalPositionalTokensEdit != nullptr
        ? additionalPositionalTokensEdit->text().trimmed()
        : QString();
    if (!updatedAdditionalPositionalTokens.isEmpty()) {
        updatedLine += QStringLiteral(" ") + updatedAdditionalPositionalTokens;
    }
    if (!validation.serializedOptions.isEmpty()) {
        updatedLine += QStringLiteral(" ") + validation.serializedOptions.join(QLatin1Char(' '));
    }

    return updatedLine;
}
}
