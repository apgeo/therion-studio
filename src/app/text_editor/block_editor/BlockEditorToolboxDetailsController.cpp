#include "BlockEditorToolboxDetailsController.h"

#include "../ContextHelpController.h"
#include "../TextEditorTab.h"

#include <QGraphicsScene>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>

namespace TherionStudio
{
BlockEditorToolboxDetailsController::BlockEditorToolboxDetailsController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorToolboxDetailsController::showToolboxCommandDetails(const QString &commandToken)
{
    if (owner_ == nullptr || owner_->tearingDown_) {
        return;
    }
    const QString normalizedCommand = owner_->normalizedDirectiveToken(commandToken);
    if (normalizedCommand.isEmpty()) {
        return;
    }

    if (owner_->blockCanvasScene_ != nullptr) {
        const QSignalBlocker signalBlocker(owner_->blockCanvasScene_);
        owner_->blockCanvasScene_->clearSelection();
    }

    owner_->blockDetailsPopulating_ = true;
    owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::Unsupported;
    owner_->blockDetailsSelectedLineNumber_ = 0;
    owner_->blockDetailsSelectedKind_ = normalizedCommand;
    owner_->blockDetailsBaseStatusText_ = TextEditorTab::tr("Command: %1").arg(normalizedCommand);
    if (owner_->blockDetailsEditPanel_ != nullptr) {
        owner_->blockDetailsEditPanel_->setVisible(false);
    }

    if (owner_->blockDetailsStatusLabel_ != nullptr) {
        owner_->blockDetailsStatusLabel_->setStyleSheet(QString());
        owner_->blockDetailsStatusLabel_->setText(owner_->blockDetailsBaseStatusText_);
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
    if (owner_->blockDetailsCommentEdit_ != nullptr) {
        owner_->blockDetailsCommentEdit_->clear();
        owner_->blockDetailsCommentEdit_->setEnabled(false);
        owner_->blockDetailsCommentEdit_->setVisible(false);
    }
    if (owner_->blockDetailsOptionsTable_ != nullptr) {
        owner_->blockDetailsOptionsTable_->clearSelection();
        owner_->blockDetailsOptionsTable_->setRowCount(0);
        owner_->blockDetailsOptionsTable_->setEnabled(false);
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
    if (owner_->blockDetailsOptionArgsLabel_ != nullptr) {
        owner_->blockDetailsOptionArgsLabel_->setVisible(false);
    }
    if (owner_->blockDetailsOptionArgsPanel_ != nullptr) {
        owner_->blockDetailsOptionArgsPanel_->setVisible(false);
    }
    if (owner_->blockDetailsLegacyConfigureButton_ != nullptr) {
        owner_->blockDetailsLegacyConfigureButton_->setVisible(false);
        owner_->blockDetailsLegacyConfigureButton_->setEnabled(false);
    }
    if (owner_->blockDetailsApplyButton_ != nullptr) {
        owner_->blockDetailsApplyButton_->setEnabled(false);
    }
    if (owner_->blockDetailsHelpBrowser_ != nullptr) {
        const TherionHelpEntry entry = owner_->helpEntries_.value(normalizedCommand);
        owner_->blockDetailsHelpBrowser_->setHtml(
            ContextHelpController::renderHelpSummaryHtml(
                normalizedCommand,
                entry.summary,
                TextEditorTab::tr("No summary is available for this command.")));
    }
    owner_->refreshBlockDetailsOptionArgumentEditors();
    owner_->blockDetailsPopulating_ = false;
}
}
