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
        context_.editPanel->setVisible(false);
    }
    context_.setDetailsPopulating(true);
    const auto tr = [](const char *text) {
        return QCoreApplication::translate("TherionStudio::BlockEditorDetailsPaneController", text);
    };
    if (context_.statusLabel != nullptr) {
        context_.statusLabel->setStyleSheet(QString());
        context_.statusLabel->setText(tr("Select a block in the canvas to edit its parameters."));
    }
    if (context_.idEdit != nullptr) {
        context_.idEdit->clear();
        context_.idEdit->setEnabled(false);
        context_.idEdit->setVisible(true);
    }
    if (context_.additionalPositionalEdit != nullptr) {
        context_.additionalPositionalEdit->clear();
        context_.additionalPositionalEdit->setEnabled(false);
        context_.additionalPositionalEdit->setVisible(true);
    }
    if (context_.secondaryFieldStack != nullptr) {
        context_.secondaryFieldStack->setVisible(true);
        if (context_.additionalPositionalEdit != nullptr) {
            context_.secondaryFieldStack->setCurrentWidget(context_.additionalPositionalEdit);
        }
    }
    if (context_.clearReadingsTagEditor) {
        context_.clearReadingsTagEditor();
    }
    if (context_.commentEdit != nullptr) {
        context_.commentEdit->clear();
        context_.commentEdit->setEnabled(false);
        context_.commentEdit->setVisible(true);
    }
    if (context_.primaryFieldLabel != nullptr) {
        context_.primaryFieldLabel->setText(tr("ID"));
        context_.primaryFieldLabel->setVisible(true);
    }
    if (context_.secondaryFieldLabel != nullptr) {
        context_.secondaryFieldLabel->setText(tr("Extra Arguments (Advanced)"));
        context_.secondaryFieldLabel->setVisible(true);
    }
    if (context_.commentFieldLabel != nullptr) {
        context_.commentFieldLabel->setText(tr("Comment"));
        context_.commentFieldLabel->setVisible(true);
    }
    if (context_.optionsTable != nullptr) {
        context_.optionsTable->setRowCount(0);
        context_.optionsTable->setEnabled(false);
        context_.optionsTable->setVisible(true);
    }
    if (context_.optionArgsLabel != nullptr) {
        context_.optionArgsLabel->setVisible(false);
    }
    if (context_.optionArgsPanel != nullptr) {
        context_.optionArgsPanel->setVisible(false);
    }
    if (context_.addOptionButton != nullptr) {
        context_.addOptionButton->setEnabled(false);
        context_.addOptionButton->setVisible(true);
    }
    if (context_.removeOptionButton != nullptr) {
        context_.removeOptionButton->setEnabled(false);
        context_.removeOptionButton->setVisible(true);
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
