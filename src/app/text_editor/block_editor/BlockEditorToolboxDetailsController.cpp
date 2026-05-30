#include "BlockEditorToolboxDetailsController.h"

#include "BlockEditorDirectiveRules.h"
#include "../ContextHelpController.h"
#include "../TextEditorCommandMetadata.h"

#include <QCoreApplication>
#include <QGraphicsScene>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>

#include <utility>

namespace TherionStudio
{
BlockEditorToolboxDetailsController::BlockEditorToolboxDetailsController(BlockEditorToolboxDetailsContext context)
    : context_(std::move(context))
{
}

void BlockEditorToolboxDetailsController::showToolboxCommandDetails(const QString &commandToken)
{
    if ((context_.tearingDown != nullptr && *context_.tearingDown)
        || !context_.normalizeDirectiveToken
        || !context_.beginToolboxCommandDetails
        || !context_.setDetailsPopulating) {
        return;
    }
    const QString normalizedCommand = context_.normalizeDirectiveToken(commandToken);
    if (normalizedCommand.isEmpty()) {
        return;
    }

    const auto tr = [](const char *text) {
        return QCoreApplication::translate("TherionStudio::BlockEditorToolboxDetailsController", text);
    };

    if (context_.scene != nullptr) {
        const QSignalBlocker signalBlocker(context_.scene);
        context_.scene->clearSelection();
    }

    const bool mapReference = BlockEditorDirectiveRules::isMapObjectReferenceKind(normalizedCommand);
    context_.beginToolboxCommandDetails(normalizedCommand,
                                        mapReference ? tr("Object Reference")
                                                     : tr("Command: %1").arg(normalizedCommand));
    if (context_.editPanel != nullptr) {
        context_.editPanel->setVisible(false);
    }

    if (context_.statusLabel != nullptr) {
        context_.statusLabel->setStyleSheet(QString());
        context_.statusLabel->setText(tr("Command: %1").arg(normalizedCommand));
    }
    if (context_.primaryFieldLabel != nullptr) {
        context_.primaryFieldLabel->setVisible(false);
    }
    if (context_.secondaryFieldLabel != nullptr) {
        context_.secondaryFieldLabel->setVisible(false);
    }
    if (context_.commentFieldLabel != nullptr) {
        context_.commentFieldLabel->setVisible(false);
    }
    if (context_.idEdit != nullptr) {
        context_.idEdit->clear();
        context_.idEdit->setEnabled(false);
        context_.idEdit->setVisible(false);
    }
    if (context_.additionalPositionalEdit != nullptr) {
        context_.additionalPositionalEdit->clear();
        context_.additionalPositionalEdit->setEnabled(false);
        context_.additionalPositionalEdit->setVisible(false);
    }
    if (context_.secondaryFieldStack != nullptr) {
        context_.secondaryFieldStack->setVisible(false);
        if (context_.additionalPositionalEdit != nullptr) {
            context_.secondaryFieldStack->setCurrentWidget(context_.additionalPositionalEdit);
        }
    }
    if (context_.commentEdit != nullptr) {
        context_.commentEdit->clear();
        context_.commentEdit->setEnabled(false);
        context_.commentEdit->setVisible(false);
    }
    if (context_.optionsTable != nullptr) {
        context_.optionsTable->clearSelection();
        context_.optionsTable->setRowCount(0);
        context_.optionsTable->setEnabled(false);
        context_.optionsTable->setVisible(false);
    }
    if (context_.addOptionButton != nullptr) {
        context_.addOptionButton->setEnabled(false);
        context_.addOptionButton->setVisible(false);
    }
    if (context_.removeOptionButton != nullptr) {
        context_.removeOptionButton->setEnabled(false);
        context_.removeOptionButton->setVisible(false);
    }
    if (context_.optionArgsLabel != nullptr) {
        context_.optionArgsLabel->setVisible(false);
    }
    if (context_.optionArgsPanel != nullptr) {
        context_.optionArgsPanel->setVisible(false);
    }
    if (context_.legacyConfigureButton != nullptr) {
        context_.legacyConfigureButton->setVisible(false);
        context_.legacyConfigureButton->setEnabled(false);
    }
    if (context_.applyButton != nullptr) {
        context_.applyButton->setEnabled(false);
    }
    if (context_.helpBrowser != nullptr && context_.commandMetadata != nullptr) {
        if (mapReference) {
            context_.helpBrowser->setHtml(tr("<p><b>Object Reference</b></p>"
                                             "<p>References an existing scrap or map from inside a <code>map</code> block. "
                                             "The block serializes as a single map body line, for example <code>scrap-a</code>.</p>"));
        } else {
            const TherionHelpEntry entry = context_.commandMetadata->helpEntries.value(normalizedCommand);
            context_.helpBrowser->setHtml(
                ContextHelpController::renderHelpSummaryHtml(
                    normalizedCommand,
                    entry.summary,
                    tr("No summary is available for this command.")));
        }
    }
    if (context_.refreshOptionArgumentEditors) {
        context_.refreshOptionArgumentEditors();
    }
    context_.setDetailsPopulating(false);
}
}
