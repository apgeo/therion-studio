#include "BlockEditorDetailsPaneController.h"

#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>

#include <utility>

namespace TherionStudio
{
BlockEditorDetailsPaneController::BlockEditorDetailsPaneController(BlockEditorDetailsPaneContext context)
    : context_(std::move(context))
{
}

void BlockEditorDetailsPaneController::clearDetailsPane()
{
    if (!context_.resetDetailsState || !context_.setDetailsPopulating) {
        return;
    }

    const bool tearingDown = context_.tearingDown != nullptr && *context_.tearingDown;
    context_.resetDetailsState(tearingDown);
    if (tearingDown) {
        return;
    }

    if (context_.editPanel != nullptr) {
        context_.editPanel->setVisible(true);
    }
    context_.setDetailsPopulating(true);
    const auto tr = [](const char *text) {
        return QCoreApplication::translate("TherionStudio::BlockEditorDetailsPaneController", text);
    };
    if (context_.titleLabel != nullptr) {
        context_.titleLabel->setText(tr("Selection"));
        context_.titleLabel->setVisible(false);
    }
    if (context_.statusLabel != nullptr) {
        context_.statusLabel->setStyleSheet(QString());
        context_.statusLabel->setText(tr("No block selected."));
    }
    if (context_.idEdit != nullptr) {
        context_.idEdit->clear();
        context_.idEdit->setReadOnly(false);
        context_.idEdit->setEnabled(false);
        context_.idEdit->setVisible(false);
    }
    if (context_.readOnlyValueLabel != nullptr) {
        context_.readOnlyValueLabel->clear();
        context_.readOnlyValueLabel->setVisible(false);
    }
    if (context_.primaryFieldStack != nullptr) {
        context_.primaryFieldStack->setVisible(false);
        if (context_.idEdit != nullptr) {
            context_.primaryFieldStack->setCurrentWidget(context_.idEdit);
        }
    }
    if (context_.additionalPositionalEdit != nullptr) {
        context_.additionalPositionalEdit->clear();
        context_.additionalPositionalEdit->setReadOnly(false);
        context_.additionalPositionalEdit->setEnabled(false);
        context_.additionalPositionalEdit->setVisible(false);
    }
    if (context_.secondaryFieldStack != nullptr) {
        context_.secondaryFieldStack->setVisible(false);
        if (context_.additionalPositionalEdit != nullptr) {
            context_.secondaryFieldStack->setCurrentWidget(context_.additionalPositionalEdit);
        }
    }
    if (context_.clearReadingsTagEditor) {
        context_.clearReadingsTagEditor();
    }
    if (context_.commentEdit != nullptr) {
        context_.commentEdit->clear();
        context_.commentEdit->setReadOnly(false);
        context_.commentEdit->setEnabled(false);
        context_.commentEdit->setVisible(false);
    }
    if (context_.primaryFieldLabel != nullptr) {
        context_.primaryFieldLabel->setText(tr("ID"));
        context_.primaryFieldLabel->setVisible(false);
    }
    if (context_.secondaryFieldLabel != nullptr) {
        context_.secondaryFieldLabel->setText(tr("Extra Arguments (Advanced)"));
        context_.secondaryFieldLabel->setVisible(false);
    }
    if (context_.commentFieldLabel != nullptr) {
        context_.commentFieldLabel->setText(tr("Comment"));
        context_.commentFieldLabel->setVisible(false);
    }
    if (context_.optionsLabel != nullptr) {
        context_.optionsLabel->setVisible(false);
    }
    if (context_.optionsTable != nullptr) {
        context_.optionsTable->setRowCount(0);
        context_.optionsTable->setEnabled(false);
        context_.optionsTable->setVisible(false);
    }
    if (context_.optionArgsLabel != nullptr) {
        context_.optionArgsLabel->setVisible(false);
    }
    if (context_.optionArgsPanel != nullptr) {
        context_.optionArgsPanel->setVisible(false);
    }
    if (context_.addOptionButton != nullptr) {
        context_.addOptionButton->setEnabled(false);
        context_.addOptionButton->setVisible(false);
    }
    if (context_.removeOptionButton != nullptr) {
        context_.removeOptionButton->setEnabled(false);
        context_.removeOptionButton->setVisible(false);
    }
    if (context_.dataRowsButton != nullptr) {
        context_.dataRowsButton->setEnabled(false);
        context_.dataRowsButton->setVisible(false);
    }
    if (context_.helpBrowser != nullptr) {
        context_.helpBrowser->setHtml(tr("<p>Select a block parameter to see contextual help.</p>"));
    }
    if (context_.refreshOptionArgumentEditors) {
        context_.refreshOptionArgumentEditors();
    }
    context_.setDetailsPopulating(false);
}
}
