#include "BlockEditorSelectionDetailsController.h"

#include "BlockEditorCommandOptionParser.h"
#include "BlockEditorDetailsSupport.h"
#include "BlockEditorSourceController.h"
#include "../ContextHelpController.h"
#include "../TextEditorTab.h"

#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextBrowser>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

namespace TherionStudio
{
BlockEditorSelectionDetailsController::BlockEditorSelectionDetailsController(TextEditorTab *owner)
    : owner_(owner)
{
}

bool BlockEditorSelectionDetailsController::loadSelectionDetails(const QString &kind, int lineNumber)
{
    if (owner_ == nullptr || owner_->tearingDown_) {
        return false;
    }
    const BlockEditorSourceController source(owner_);
    if (lineNumber <= 0 || !source.hasEditor()) {
        owner_->clearBlockDetailsPane();
        return false;
    }

    QStringList lines = source.normalizedLines();
    if (lineNumber > lines.size()) {
        owner_->clearBlockDetailsPane();
        return false;
    }

    const QString normalizedKind = owner_->normalizedDirectiveToken(kind);
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineNumber - 1), lineNumber);
    if (parsedLine.tokens.isEmpty()) {
        owner_->clearBlockDetailsPane();
        return false;
    }
    const QString inlineComment = parsedLine.commentStart >= 0 ? parsedLine.commentText.trimmed() : QString();
    if (parsedLine.commentStart >= 0 && parsedLine.commentStart < lines.at(lineNumber - 1).size()) {
        owner_->blockDetailsCommentMarker_ = lines.at(lineNumber - 1).at(parsedLine.commentStart);
    } else {
        owner_->blockDetailsCommentMarker_ = QLatin1Char('#');
    }

    owner_->blockDetailsSelectedLineNumber_ = lineNumber;
    owner_->blockDetailsSelectedKind_ = normalizedKind;
    if (owner_->blockDetailsEditPanel_ != nullptr) {
        owner_->blockDetailsEditPanel_->setVisible(true);
    }

    owner_->blockDetailsPopulating_ = true;

    const bool supported = owner_->supportsDetailsPaneForKind(normalizedKind);
    const bool hasCatalogOptions = !owner_->commandMetadata().commandOptionTokens.value(normalizedKind).isEmpty();
    const bool structuredOptionsMode = owner_->isContainerBlockDirectiveForBlocks(normalizedKind) || hasCatalogOptions;
    const bool simpleValueMode = !structuredOptionsMode && normalizedKind != QStringLiteral("data");
    const bool dataHeaderMode = normalizedKind == QStringLiteral("data");
    if (!supported) {
        owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::Unsupported;
    } else if (structuredOptionsMode) {
        owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::StructuredOptions;
    } else if (simpleValueMode) {
        owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::SimpleValue;
    } else if (dataHeaderMode) {
        owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::DataHeader;
    } else {
        owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::Unsupported;
    }

    owner_->blockDetailsBaseStatusText_ = TextEditorTab::tr("Command: %1").arg(normalizedKind);
    if (owner_->blockDetailsStatusLabel_ != nullptr) {
        owner_->blockDetailsStatusLabel_->setStyleSheet(QString());
        owner_->blockDetailsStatusLabel_->setText(owner_->blockDetailsBaseStatusText_);
    }

    if (!supported) {
        if (owner_->blockDetailsEditPanel_ != nullptr) {
            owner_->blockDetailsEditPanel_->setVisible(false);
        }
        if (owner_->blockDetailsPrimaryFieldLabel_ != nullptr) {
            owner_->blockDetailsPrimaryFieldLabel_->setVisible(false);
        }
        if (owner_->blockDetailsSecondaryFieldLabel_ != nullptr) {
            owner_->blockDetailsSecondaryFieldLabel_->setVisible(false);
        }
        if (owner_->blockDetailsCommentFieldLabel_ != nullptr) {
            owner_->blockDetailsCommentFieldLabel_->setVisible(false);
        }
        if (owner_->blockDetailsIdEdit_ != nullptr) {
            owner_->blockDetailsIdEdit_->clear();
            owner_->blockDetailsIdEdit_->setEnabled(false);
            owner_->blockDetailsIdEdit_->setVisible(false);
        }
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
            owner_->blockDetailsAdditionalPositionalEdit_->clear();
            owner_->blockDetailsAdditionalPositionalEdit_->setEnabled(false);
            owner_->blockDetailsAdditionalPositionalEdit_->setVisible(false);
        }
        if (owner_->blockDetailsSecondaryFieldStack_ != nullptr) {
            owner_->blockDetailsSecondaryFieldStack_->setVisible(false);
            if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
                owner_->blockDetailsSecondaryFieldStack_->setCurrentWidget(owner_->blockDetailsAdditionalPositionalEdit_);
            }
        }
        owner_->setBlockDetailsReadingsTagEditor(QString(), {}, {});
        if (owner_->blockDetailsCommentEdit_ != nullptr) {
            owner_->blockDetailsCommentEdit_->clear();
            owner_->blockDetailsCommentEdit_->setEnabled(false);
            owner_->blockDetailsCommentEdit_->setVisible(false);
        }
        if (owner_->blockDetailsOptionsLabel_ != nullptr) {
            owner_->blockDetailsOptionsLabel_->setVisible(false);
        }
        if (owner_->blockDetailsOptionsTable_ != nullptr) {
            owner_->blockDetailsOptionsTable_->clearSelection();
            owner_->blockDetailsOptionsTable_->setEnabled(false);
            owner_->blockDetailsOptionsTable_->setRowCount(0);
            owner_->blockDetailsOptionsTable_->setVisible(false);
        }
        if (owner_->blockDetailsAddOptionButton_ != nullptr) {
            owner_->blockDetailsAddOptionButton_->setEnabled(false);
            owner_->blockDetailsAddOptionButton_->setVisible(false);
        }
        if (owner_->blockDetailsRemoveOptionButton_ != nullptr) {
            owner_->blockDetailsRemoveOptionButton_->setEnabled(false);
            owner_->blockDetailsRemoveOptionButton_->setVisible(false);
        }
        if (owner_->blockDetailsLegacyConfigureButton_ != nullptr) {
            owner_->blockDetailsLegacyConfigureButton_->setEnabled(false);
            owner_->blockDetailsLegacyConfigureButton_->setVisible(false);
        }
        if (owner_->blockDetailsOptionArgsLabel_ != nullptr) {
            owner_->blockDetailsOptionArgsLabel_->setVisible(false);
        }
        if (owner_->blockDetailsOptionArgsPanel_ != nullptr) {
            owner_->blockDetailsOptionArgsPanel_->setVisible(false);
        }
        if (owner_->blockDetailsApplyButton_ != nullptr) {
            owner_->blockDetailsApplyButton_->setEnabled(false);
        }
        owner_->blockDetailsPopulating_ = false;
        owner_->refreshBlockDetailsOptionArgumentEditors();
        owner_->updateBlockDetailsHelpForCurrentFocus();
        owner_->refreshBlockDetailsApplyState();
        return true;
    }

    if (owner_->blockDetailsPrimaryFieldLabel_ != nullptr) {
        owner_->blockDetailsPrimaryFieldLabel_->setVisible(true);
    }
    if (owner_->blockDetailsSecondaryFieldLabel_ != nullptr) {
        owner_->blockDetailsSecondaryFieldLabel_->setVisible(true);
    }
    if (owner_->blockDetailsCommentFieldLabel_ != nullptr) {
        owner_->blockDetailsCommentFieldLabel_->setText(TextEditorTab::tr("Comment"));
        owner_->blockDetailsCommentFieldLabel_->setVisible(true);
    }
    if (owner_->blockDetailsIdEdit_ != nullptr) {
        owner_->blockDetailsIdEdit_->setVisible(true);
        owner_->installBlockDetailsLineEditCompleter(owner_->blockDetailsIdEdit_, {});
    }
    if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
        owner_->blockDetailsAdditionalPositionalEdit_->setVisible(true);
        owner_->installBlockDetailsLineEditCompleter(owner_->blockDetailsAdditionalPositionalEdit_, {});
    }
    if (owner_->blockDetailsSecondaryFieldStack_ != nullptr) {
        owner_->blockDetailsSecondaryFieldStack_->setVisible(true);
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
            owner_->blockDetailsSecondaryFieldStack_->setCurrentWidget(owner_->blockDetailsAdditionalPositionalEdit_);
        }
    }
    if (owner_->blockDetailsCommentEdit_ != nullptr) {
        owner_->blockDetailsCommentEdit_->setVisible(true);
        owner_->blockDetailsCommentEdit_->setEnabled(true);
        owner_->blockDetailsCommentEdit_->setPlaceholderText(TextEditorTab::tr("optional"));
        owner_->blockDetailsCommentEdit_->setText(inlineComment);
    }
    if (owner_->blockDetailsOptionsLabel_ != nullptr) {
        owner_->blockDetailsOptionsLabel_->setVisible(false);
    }
    if (owner_->blockDetailsOptionsTable_ != nullptr) {
        owner_->blockDetailsOptionsTable_->setEnabled(false);
        owner_->blockDetailsOptionsTable_->setRowCount(0);
        owner_->blockDetailsOptionsTable_->setVisible(false);
    }
    if (owner_->blockDetailsOptionArgsLabel_ != nullptr) {
        owner_->blockDetailsOptionArgsLabel_->setVisible(false);
    }
    if (owner_->blockDetailsOptionArgsPanel_ != nullptr) {
        owner_->blockDetailsOptionArgsPanel_->setVisible(false);
    }
    if (owner_->blockDetailsAddOptionButton_ != nullptr) {
        owner_->blockDetailsAddOptionButton_->setEnabled(false);
        owner_->blockDetailsAddOptionButton_->setVisible(false);
    }
    if (owner_->blockDetailsRemoveOptionButton_ != nullptr) {
        owner_->blockDetailsRemoveOptionButton_->setEnabled(false);
        owner_->blockDetailsRemoveOptionButton_->setVisible(false);
    }
    if (owner_->blockDetailsApplyButton_ != nullptr) {
        owner_->blockDetailsApplyButton_->setEnabled(false);
    }

    if (owner_->blockDetailsLegacyConfigureButton_ != nullptr) {
        if (!supported) {
            owner_->blockDetailsLegacyConfigureButton_->setText(TextEditorTab::tr("Legacy Configure..."));
            owner_->blockDetailsLegacyConfigureButton_->setVisible(true);
            owner_->blockDetailsLegacyConfigureButton_->setEnabled(true);
        } else if (dataHeaderMode) {
            owner_->blockDetailsLegacyConfigureButton_->setText(TextEditorTab::tr("Edit Data Rows..."));
            owner_->blockDetailsLegacyConfigureButton_->setVisible(true);
            owner_->blockDetailsLegacyConfigureButton_->setEnabled(true);
        } else {
            owner_->blockDetailsLegacyConfigureButton_->setVisible(false);
            owner_->blockDetailsLegacyConfigureButton_->setEnabled(false);
        }
    }

    if (!supported) {
        if (owner_->blockDetailsIdEdit_ != nullptr) {
            owner_->blockDetailsIdEdit_->clear();
            owner_->blockDetailsIdEdit_->setEnabled(false);
            owner_->blockDetailsIdEdit_->setPlaceholderText(QString());
        }
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
            owner_->blockDetailsAdditionalPositionalEdit_->clear();
            owner_->blockDetailsAdditionalPositionalEdit_->setEnabled(false);
        }
        owner_->setBlockDetailsReadingsTagEditor(QString(), {}, {});
        if (owner_->blockDetailsHelpBrowser_ != nullptr) {
            const TherionHelpEntry entry = owner_->commandMetadata().helpEntries.value(normalizedKind);
            QString html = TextEditorTab::tr("<p>This block currently uses legacy dialog-based configuration.</p>");
            html += ContextHelpController::renderHelpHtml(normalizedKind,
                                                          entry.summary,
                                                          entry.syntax,
                                                          entry.arguments,
                                                          entry.acceptedValues,
                                                          entry.options,
                                                          false);
            owner_->blockDetailsHelpBrowser_->setHtml(html);
        }
        owner_->blockDetailsPopulating_ = false;
        return false;
    }

    if (structuredOptionsMode) {
        const bool explicitIdMode = owner_->commandSupportsInlineIdField(normalizedKind);
        const bool requiresId = owner_->commandHasRequiredIdArgument(normalizedKind);

        const BlockEditorParsedCommandOptions parsedOptions =
            parseBlockEditorCommandOptions(normalizedKind,
                                           parsedLine.tokens,
                                           owner_->commandMetadata().commandOptionFixedArityByKey,
                                           true);

        if (owner_->blockDetailsPrimaryFieldLabel_ != nullptr) {
            if (explicitIdMode) {
                owner_->blockDetailsPrimaryFieldLabel_->setText(TextEditorTab::tr("ID"));
            } else {
                const QStringList argumentSignatures = owner_->commandArgumentSignaturesFor(normalizedKind);
                if (!argumentSignatures.isEmpty()) {
                    owner_->blockDetailsPrimaryFieldLabel_->setText(blockArgumentLabelFromSignature(argumentSignatures.first()));
                } else {
                    owner_->blockDetailsPrimaryFieldLabel_->setText(TextEditorTab::tr("Value"));
                }
            }
        }
        if (owner_->blockDetailsIdEdit_ != nullptr) {
            owner_->blockDetailsIdEdit_->setEnabled(true);
            if (explicitIdMode) {
                owner_->blockDetailsIdEdit_->setPlaceholderText(requiresId ? TextEditorTab::tr("required")
                                                                           : TextEditorTab::tr("optional"));
            } else {
                owner_->blockDetailsIdEdit_->setPlaceholderText(TextEditorTab::tr("optional"));
            }
            owner_->blockDetailsIdEdit_->setText(parsedOptions.leadingValue);
        }
        const bool showExtraArguments = !parsedOptions.extraPositionalTokens.isEmpty();
        if (owner_->blockDetailsSecondaryFieldLabel_ != nullptr) {
            owner_->blockDetailsSecondaryFieldLabel_->setText(TextEditorTab::tr("Extra Arguments (Advanced)"));
            owner_->blockDetailsSecondaryFieldLabel_->setVisible(showExtraArguments);
        }
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
            owner_->blockDetailsAdditionalPositionalEdit_->setEnabled(showExtraArguments);
            owner_->blockDetailsAdditionalPositionalEdit_->setVisible(showExtraArguments);
            owner_->blockDetailsAdditionalPositionalEdit_->setText(showExtraArguments
                                                                       ? parsedOptions.extraPositionalTokens.join(QLatin1Char(' '))
                                                                       : QString());
        }
        if (owner_->blockDetailsSecondaryFieldStack_ != nullptr) {
            owner_->blockDetailsSecondaryFieldStack_->setVisible(showExtraArguments);
            if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
                owner_->blockDetailsSecondaryFieldStack_->setCurrentWidget(owner_->blockDetailsAdditionalPositionalEdit_);
            }
        }
        owner_->setBlockDetailsReadingsTagEditor(QString(), {}, {});
        const bool showOptionsSection = hasCatalogOptions || !parsedOptions.optionEntries.isEmpty();
        if (owner_->blockDetailsOptionsLabel_ != nullptr) {
            owner_->blockDetailsOptionsLabel_->setVisible(showOptionsSection);
        }
        if (owner_->blockDetailsOptionsTable_ != nullptr) {
            owner_->blockDetailsOptionsTable_->setVisible(showOptionsSection);
            owner_->blockDetailsOptionsTable_->setEnabled(showOptionsSection);
            owner_->blockDetailsOptionsTable_->setRowCount(showOptionsSection ? parsedOptions.optionEntries.size() : 0);
            if (showOptionsSection) {
                for (int row = 0; row < parsedOptions.optionEntries.size(); ++row) {
                    owner_->blockDetailsOptionsTable_->setItem(row, 0, new QTableWidgetItem(parsedOptions.optionEntries.at(row).key));
                    owner_->blockDetailsOptionsTable_->setItem(row, 1, new QTableWidgetItem(parsedOptions.optionEntries.at(row).value));
                }
            }
            if (showOptionsSection && parsedOptions.optionEntries.size() > 0) {
                owner_->blockDetailsOptionsTable_->setCurrentCell(0, 0);
            }
        }
        if (owner_->blockDetailsAddOptionButton_ != nullptr) {
            owner_->blockDetailsAddOptionButton_->setVisible(showOptionsSection);
            owner_->blockDetailsAddOptionButton_->setEnabled(showOptionsSection);
        }
        if (owner_->blockDetailsRemoveOptionButton_ != nullptr) {
            owner_->blockDetailsRemoveOptionButton_->setVisible(showOptionsSection);
            owner_->blockDetailsRemoveOptionButton_->setEnabled(showOptionsSection && !parsedOptions.optionEntries.isEmpty());
        }
    } else if (simpleValueMode) {
        const TherionHelpEntry helpEntry = owner_->commandMetadata().helpEntries.value(normalizedKind);
        QStringList argumentSignatures;
        for (const QString &argumentLine : helpEntry.arguments) {
            const QString signature = blockArgumentSignatureFromHelpLine(argumentLine);
            if (!signature.isEmpty()) {
                argumentSignatures.append(signature);
            }
        }
        const bool hasSecondaryArgument = argumentSignatures.size() > 1;

        QString currentValue = parsedLine.tokens.size() > 1 ? parsedLine.tokens.at(1) : QString();
        QString secondaryValue;
        if (hasSecondaryArgument && parsedLine.tokens.size() > 2) {
            secondaryValue = parsedLine.tokens.mid(2).join(QLatin1Char(' '));
        } else if (!hasSecondaryArgument) {
            currentValue = parsedLine.tokens.size() > 1
                ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
                : QString();
        }

        if (owner_->blockDetailsPrimaryFieldLabel_ != nullptr) {
            if (owner_->commandMetadata().commandPrimaryValueIsPerson.value(normalizedKind, false)) {
                owner_->blockDetailsPrimaryFieldLabel_->setText(TextEditorTab::tr("Person"));
            } else if (!argumentSignatures.isEmpty()) {
                owner_->blockDetailsPrimaryFieldLabel_->setText(blockArgumentLabelFromSignature(argumentSignatures.first()));
            } else {
                owner_->blockDetailsPrimaryFieldLabel_->setText(TextEditorTab::tr("Value"));
            }
        }
        if (owner_->blockDetailsSecondaryFieldLabel_ != nullptr) {
            if (hasSecondaryArgument) {
                if (argumentSignatures.size() > 1) {
                    owner_->blockDetailsSecondaryFieldLabel_->setText(blockArgumentLabelFromSignature(argumentSignatures.at(1)));
                } else {
                    owner_->blockDetailsSecondaryFieldLabel_->setText(TextEditorTab::tr("Value 2"));
                }
                owner_->blockDetailsSecondaryFieldLabel_->setVisible(true);
            } else {
                owner_->blockDetailsSecondaryFieldLabel_->setVisible(false);
            }
        }
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
            if (hasSecondaryArgument) {
                owner_->blockDetailsAdditionalPositionalEdit_->setEnabled(true);
                owner_->blockDetailsAdditionalPositionalEdit_->setVisible(true);
                owner_->blockDetailsAdditionalPositionalEdit_->setPlaceholderText(TextEditorTab::tr("optional"));
                owner_->blockDetailsAdditionalPositionalEdit_->setText(secondaryValue);
            } else {
                owner_->blockDetailsAdditionalPositionalEdit_->clear();
                owner_->blockDetailsAdditionalPositionalEdit_->setEnabled(false);
                owner_->blockDetailsAdditionalPositionalEdit_->setVisible(false);
            }
        }
        if (owner_->blockDetailsSecondaryFieldStack_ != nullptr) {
            owner_->blockDetailsSecondaryFieldStack_->setVisible(hasSecondaryArgument);
            if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
                owner_->blockDetailsSecondaryFieldStack_->setCurrentWidget(owner_->blockDetailsAdditionalPositionalEdit_);
            }
        }
        owner_->setBlockDetailsReadingsTagEditor(QString(), {}, {});
        if (owner_->blockDetailsIdEdit_ != nullptr) {
            owner_->blockDetailsIdEdit_->setEnabled(true);
            owner_->blockDetailsIdEdit_->setPlaceholderText(TextEditorTab::tr("required"));
            owner_->blockDetailsIdEdit_->setText(currentValue);
        }
        if (owner_->blockDetailsOptionsTable_ != nullptr) {
            owner_->blockDetailsOptionsTable_->setVisible(false);
        }
        if (owner_->blockDetailsOptionsLabel_ != nullptr) {
            owner_->blockDetailsOptionsLabel_->setVisible(false);
        }
        if (owner_->blockDetailsAddOptionButton_ != nullptr) {
            owner_->blockDetailsAddOptionButton_->setVisible(false);
        }
        if (owner_->blockDetailsRemoveOptionButton_ != nullptr) {
            owner_->blockDetailsRemoveOptionButton_->setVisible(false);
        }
    } else if (dataHeaderMode) {
        const BlockEditorDataHeaderComponents components = parseBlockEditorDataHeaderComponents(parsedLine.tokens);
        const QStringList styleSuggestions = owner_->commandMetadata().commandArgumentValueTokens.value(commandArgumentValueKey(QStringLiteral("data"), 0));
        const QStringList readingSuggestions = owner_->commandMetadata().commandArgumentValueTokens.value(commandArgumentValueKey(QStringLiteral("data"), 1));
        if (owner_->blockDetailsPrimaryFieldLabel_ != nullptr) {
            owner_->blockDetailsPrimaryFieldLabel_->setText(TextEditorTab::tr("Style"));
        }
        if (owner_->blockDetailsSecondaryFieldLabel_ != nullptr) {
            owner_->blockDetailsSecondaryFieldLabel_->setText(TextEditorTab::tr("Readings Order"));
            owner_->blockDetailsSecondaryFieldLabel_->setVisible(true);
        }
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
            owner_->blockDetailsAdditionalPositionalEdit_->setEnabled(true);
            owner_->blockDetailsAdditionalPositionalEdit_->setVisible(false);
            owner_->blockDetailsAdditionalPositionalEdit_->setText(components.readingsOrder);
        }
        if (owner_->blockDetailsSecondaryFieldStack_ != nullptr) {
            owner_->blockDetailsSecondaryFieldStack_->setVisible(true);
            if (owner_->blockDetailsReadingsTagEditor_ != nullptr) {
                owner_->blockDetailsSecondaryFieldStack_->setCurrentWidget(owner_->blockDetailsReadingsTagEditor_);
            }
        }
        owner_->setBlockDetailsReadingsTagEditor(TextEditorTab::tr("Type token and press Enter/Space"),
                                                 readingSuggestions,
                                                 TherionDocumentParser::tokenizeLine(components.readingsOrder));
        if (owner_->blockDetailsIdEdit_ != nullptr) {
            owner_->blockDetailsIdEdit_->setEnabled(true);
            owner_->blockDetailsIdEdit_->setPlaceholderText(TextEditorTab::tr("required"));
            owner_->blockDetailsIdEdit_->setText(components.style);
            owner_->installBlockDetailsLineEditCompleter(owner_->blockDetailsIdEdit_, styleSuggestions);
        }
        if (owner_->blockDetailsOptionsTable_ != nullptr) {
            owner_->blockDetailsOptionsTable_->setVisible(false);
        }
        if (owner_->blockDetailsOptionsLabel_ != nullptr) {
            owner_->blockDetailsOptionsLabel_->setVisible(false);
        }
        if (owner_->blockDetailsAddOptionButton_ != nullptr) {
            owner_->blockDetailsAddOptionButton_->setVisible(false);
        }
        if (owner_->blockDetailsRemoveOptionButton_ != nullptr) {
            owner_->blockDetailsRemoveOptionButton_->setVisible(false);
        }
    }

    owner_->blockDetailsPopulating_ = false;
    owner_->refreshBlockDetailsOptionArgumentEditors();
    owner_->updateBlockDetailsHelpForCurrentFocus();
    owner_->refreshBlockDetailsApplyState();
    return true;
}
}
