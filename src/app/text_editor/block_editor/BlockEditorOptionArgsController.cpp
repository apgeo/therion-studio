#include "BlockEditorOptionArgsController.h"

#include "../TextEditorTab.h"

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace TherionStudio
{
BlockEditorOptionArgsController::BlockEditorOptionArgsController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorOptionArgsController::refreshOptionArgumentEditors()
{
    if (owner_ == nullptr
        || owner_->blockDetailsOptionArgsLabel_ == nullptr
        || owner_->blockDetailsOptionArgsPanel_ == nullptr
        || owner_->blockDetailsOptionArgsFormLayout_ == nullptr
        || owner_->blockDetailsOptionsTable_ == nullptr) {
        return;
    }

    auto clearEditors = [this]() {
        owner_->blockDetailsOptionArgsSyncing_ = true;
        owner_->blockDetailsOptionArgEditors_.clear();
        while (owner_->blockDetailsOptionArgsFormLayout_->rowCount() > 0) {
            QFormLayout::TakeRowResult row = owner_->blockDetailsOptionArgsFormLayout_->takeRow(0);
            if (row.labelItem != nullptr) {
                if (QWidget *widget = row.labelItem->widget(); widget != nullptr) {
                    widget->deleteLater();
                }
                delete row.labelItem;
            }
            if (row.fieldItem != nullptr) {
                if (QWidget *widget = row.fieldItem->widget(); widget != nullptr) {
                    widget->deleteLater();
                }
                delete row.fieldItem;
            }
        }
        owner_->blockDetailsOptionArgsSyncing_ = false;
    };

    const bool supportedMode = owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::StructuredOptions
        && owner_->blockDetailsOptionsTable_->isVisible()
        && owner_->blockDetailsOptionsTable_->isEnabled();
    if (!supportedMode) {
        clearEditors();
        owner_->blockDetailsOptionArgsLabel_->setVisible(false);
        owner_->blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    auto updateOptionValueCellEditability = [this]() {
        if (owner_->blockDetailsOptionsTable_ == nullptr) {
            return;
        }
        QSignalBlocker tableSignalBlocker(owner_->blockDetailsOptionsTable_);
        const QString commandToken = owner_->normalizedDirectiveToken(owner_->blockDetailsSelectedKind_);
        const QString multiValueToolTip
            = TextEditorTab::tr("Use Selected Option Parameters below to edit this multi-value option.");
        for (int optionRow = 0; optionRow < owner_->blockDetailsOptionsTable_->rowCount(); ++optionRow) {
            QTableWidgetItem *valueItem = owner_->blockDetailsOptionsTable_->item(optionRow, 1);
            if (valueItem == nullptr) {
                valueItem = new QTableWidgetItem(QString());
                owner_->blockDetailsOptionsTable_->setItem(optionRow, 1, valueItem);
            }
            const QString optionToken = (owner_->blockDetailsOptionsTable_->item(optionRow, 0) != nullptr
                                             ? owner_->blockDetailsOptionsTable_->item(optionRow, 0)->text()
                                             : QString())
                                            .trimmed()
                                            .toLower();
            const int fixedArity = owner_->commandMetadata().commandOptionFixedArityByKey.value(
                commandOptionValueKey(commandToken, optionToken), -1);
            Qt::ItemFlags flags = valueItem->flags();
            if (fixedArity > 1) {
                flags &= ~Qt::ItemIsEditable;
                if (valueItem->toolTip() != multiValueToolTip) {
                    valueItem->setToolTip(multiValueToolTip);
                }
            } else {
                flags |= Qt::ItemIsEditable;
                if (!valueItem->toolTip().isEmpty()) {
                    valueItem->setToolTip(QString());
                }
            }
            if (valueItem->flags() != flags) {
                valueItem->setFlags(flags);
            }
        }
    };

    updateOptionValueCellEditability();

    const int row = owner_->blockDetailsOptionsTable_->currentRow();
    if (row < 0 || row >= owner_->blockDetailsOptionsTable_->rowCount()) {
        clearEditors();
        owner_->blockDetailsOptionArgsLabel_->setVisible(false);
        owner_->blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    const QString commandToken = owner_->normalizedDirectiveToken(owner_->blockDetailsSelectedKind_);
    const QString optionToken = (owner_->blockDetailsOptionsTable_->item(row, 0) != nullptr
                                     ? owner_->blockDetailsOptionsTable_->item(row, 0)->text()
                                     : QString())
                                    .trimmed()
                                    .toLower();
    if (optionToken.isEmpty()) {
        clearEditors();
        owner_->blockDetailsOptionArgsLabel_->setVisible(false);
        owner_->blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    const QString optionKey = commandOptionValueKey(commandToken, optionToken);
    const int fixedArity = owner_->commandMetadata().commandOptionFixedArityByKey.value(optionKey, -1);
    if (fixedArity <= 1) {
        clearEditors();
        owner_->blockDetailsOptionArgsLabel_->setVisible(false);
        owner_->blockDetailsOptionArgsPanel_->setVisible(false);
        return;
    }

    QStringList argumentLabels = owner_->commandMetadata().commandOptionArgumentLabelsByKey.value(optionKey);
    while (argumentLabels.size() < fixedArity) {
        argumentLabels.append(TextEditorTab::tr("Value %1").arg(argumentLabels.size() + 1));
    }
    if (argumentLabels.size() > fixedArity) {
        argumentLabels = argumentLabels.mid(0, fixedArity);
    }

    QStringList valueTokens;
    const QString valueText = (owner_->blockDetailsOptionsTable_->item(row, 1) != nullptr
                                   ? owner_->blockDetailsOptionsTable_->item(row, 1)->text()
                                   : QString())
                                  .trimmed();
    if (!valueText.isEmpty()) {
        valueTokens = TherionDocumentParser::tokenizeLine(valueText);
        if (valueTokens.isEmpty()) {
            valueTokens.append(valueText);
        }
    }

    clearEditors();
    owner_->blockDetailsOptionArgsSyncing_ = true;
    for (int index = 0; index < fixedArity; ++index) {
        auto *edit = new QLineEdit(owner_->blockDetailsOptionArgsPanel_);
        edit->setPlaceholderText(TextEditorTab::tr("required"));
        edit->setText(index < valueTokens.size() ? valueTokens.at(index) : QString());
        owner_->blockDetailsOptionArgsFormLayout_->addRow(argumentLabels.at(index), edit);
        owner_->blockDetailsOptionArgEditors_.append(edit);

        QObject::connect(edit, &QLineEdit::textChanged, owner_, [this, optionKey, row](const QString &) {
            if (owner_->blockDetailsOptionArgsSyncing_
                || owner_->blockDetailsOptionsTable_ == nullptr
                || row < 0
                || row >= owner_->blockDetailsOptionsTable_->rowCount()) {
                return;
            }

            const QString selectedOptionToken = (owner_->blockDetailsOptionsTable_->item(row, 0) != nullptr
                                                     ? owner_->blockDetailsOptionsTable_->item(row, 0)->text()
                                                     : QString())
                                                    .trimmed()
                                                    .toLower();
            if (commandOptionValueKey(owner_->normalizedDirectiveToken(owner_->blockDetailsSelectedKind_),
                                      selectedOptionToken)
                != optionKey) {
                return;
            }

            QStringList serializedValues;
            serializedValues.reserve(owner_->blockDetailsOptionArgEditors_.size());
            for (QLineEdit *valueEdit : owner_->blockDetailsOptionArgEditors_) {
                const QString rawValue = valueEdit != nullptr ? valueEdit->text().trimmed() : QString();
                serializedValues.append(serializeTherionArgumentToken(rawValue));
            }

            QSignalBlocker blocker(owner_->blockDetailsOptionsTable_);
            owner_->blockDetailsOptionArgsSyncing_ = true;
            QTableWidgetItem *valueItem = owner_->blockDetailsOptionsTable_->item(row, 1);
            if (valueItem == nullptr) {
                valueItem = new QTableWidgetItem;
                owner_->blockDetailsOptionsTable_->setItem(row, 1, valueItem);
            }
            valueItem->setText(serializedValues.join(QLatin1Char(' ')));
            owner_->blockDetailsOptionArgsSyncing_ = false;

            owner_->refreshBlockDetailsApplyState();
            owner_->updateBlockDetailsHelpForCurrentFocus();
        });
        QObject::connect(edit, &QLineEdit::selectionChanged, owner_, [this]() {
            owner_->updateBlockDetailsHelpForCurrentFocus();
        });
    }
    owner_->blockDetailsOptionArgsSyncing_ = false;

    owner_->blockDetailsOptionArgsLabel_->setVisible(true);
    owner_->blockDetailsOptionArgsPanel_->setVisible(true);
}
}
