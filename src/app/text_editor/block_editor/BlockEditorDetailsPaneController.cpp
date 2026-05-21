#include "BlockEditorDetailsPaneController.h"

#include "../TextEditorTab.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>

namespace TherionStudio
{
BlockEditorDetailsPaneController::BlockEditorDetailsPaneController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorDetailsPaneController::clearDetailsPane()
{
    if (owner_ == nullptr) {
        return;
    }

    if (owner_->tearingDown_) {
        owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::None;
        owner_->blockDetailsSelectedLineNumber_ = 0;
        owner_->blockDetailsSelectedKind_.clear();
        return;
    }

    owner_->blockDetailsMode_ = TextEditorTab::BlockDetailsMode::None;
    owner_->blockDetailsSelectedLineNumber_ = 0;
    owner_->blockDetailsSelectedKind_.clear();
    owner_->blockDetailsBaseStatusText_.clear();
    if (owner_->blockDetailsEditPanel_ != nullptr) {
        owner_->blockDetailsEditPanel_->setVisible(false);
    }
    owner_->blockDetailsPopulating_ = true;
    if (owner_->blockDetailsStatusLabel_ != nullptr) {
        owner_->blockDetailsStatusLabel_->setStyleSheet(QString());
        owner_->blockDetailsStatusLabel_->setText(TextEditorTab::tr("Select a block in the canvas to edit its parameters."));
    }
    if (owner_->blockDetailsIdEdit_ != nullptr) {
        owner_->blockDetailsIdEdit_->clear();
        owner_->blockDetailsIdEdit_->setEnabled(false);
        owner_->blockDetailsIdEdit_->setVisible(true);
    }
    if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
        owner_->blockDetailsAdditionalPositionalEdit_->clear();
        owner_->blockDetailsAdditionalPositionalEdit_->setEnabled(false);
        owner_->blockDetailsAdditionalPositionalEdit_->setVisible(true);
    }
    if (owner_->blockDetailsSecondaryFieldStack_ != nullptr) {
        owner_->blockDetailsSecondaryFieldStack_->setVisible(true);
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr) {
            owner_->blockDetailsSecondaryFieldStack_->setCurrentWidget(owner_->blockDetailsAdditionalPositionalEdit_);
        }
    }
    owner_->setBlockDetailsReadingsTagEditor(QString(), {}, {});
    if (owner_->blockDetailsCommentEdit_ != nullptr) {
        owner_->blockDetailsCommentEdit_->clear();
        owner_->blockDetailsCommentEdit_->setEnabled(false);
        owner_->blockDetailsCommentEdit_->setVisible(true);
    }
    if (owner_->blockDetailsPrimaryFieldLabel_ != nullptr) {
        owner_->blockDetailsPrimaryFieldLabel_->setText(TextEditorTab::tr("ID"));
        owner_->blockDetailsPrimaryFieldLabel_->setVisible(true);
    }
    if (owner_->blockDetailsSecondaryFieldLabel_ != nullptr) {
        owner_->blockDetailsSecondaryFieldLabel_->setText(TextEditorTab::tr("Extra Arguments (Advanced)"));
        owner_->blockDetailsSecondaryFieldLabel_->setVisible(true);
    }
    if (owner_->blockDetailsCommentFieldLabel_ != nullptr) {
        owner_->blockDetailsCommentFieldLabel_->setText(TextEditorTab::tr("Comment"));
        owner_->blockDetailsCommentFieldLabel_->setVisible(true);
    }
    if (owner_->blockDetailsOptionsTable_ != nullptr) {
        owner_->blockDetailsOptionsTable_->setRowCount(0);
        owner_->blockDetailsOptionsTable_->setEnabled(false);
        owner_->blockDetailsOptionsTable_->setVisible(true);
    }
    if (owner_->blockDetailsOptionArgsLabel_ != nullptr) {
        owner_->blockDetailsOptionArgsLabel_->setVisible(false);
    }
    if (owner_->blockDetailsOptionArgsPanel_ != nullptr) {
        owner_->blockDetailsOptionArgsPanel_->setVisible(false);
    }
    if (owner_->blockDetailsAddOptionButton_ != nullptr) {
        owner_->blockDetailsAddOptionButton_->setEnabled(false);
        owner_->blockDetailsAddOptionButton_->setVisible(true);
    }
    if (owner_->blockDetailsRemoveOptionButton_ != nullptr) {
        owner_->blockDetailsRemoveOptionButton_->setEnabled(false);
        owner_->blockDetailsRemoveOptionButton_->setVisible(true);
    }
    if (owner_->blockDetailsApplyButton_ != nullptr) {
        owner_->blockDetailsApplyButton_->setEnabled(false);
    }
    if (owner_->blockDetailsLegacyConfigureButton_ != nullptr) {
        owner_->blockDetailsLegacyConfigureButton_->setEnabled(false);
        owner_->blockDetailsLegacyConfigureButton_->setVisible(false);
    }
    if (owner_->blockDetailsHelpBrowser_ != nullptr) {
        owner_->blockDetailsHelpBrowser_->setHtml(TextEditorTab::tr("<p>Select a block parameter to see contextual help.</p>"));
    }
    owner_->refreshBlockDetailsOptionArgumentEditors();
    owner_->blockDetailsPopulating_ = false;
}
}
