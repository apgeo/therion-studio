#include "BlockEditorDetailsHelpController.h"

#include "../ContextHelpController.h"
#include "../TextEditorTab.h"

#include <QLabel>
#include <QLineEdit>
#include <QTextBrowser>

namespace TherionStudio
{
BlockEditorDetailsHelpController::BlockEditorDetailsHelpController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorDetailsHelpController::updateHelpForCurrentFocus()
{
    if (owner_ == nullptr || owner_->tearingDown_) {
        return;
    }
    if (owner_->blockDetailsHelpBrowser_ == nullptr) {
        return;
    }
    if (owner_->blockDetailsSelectedKind_.isEmpty()) {
        owner_->blockDetailsHelpBrowser_->setHtml(TextEditorTab::tr("<p>Select a block parameter to see contextual help.</p>"));
        return;
    }

    const QString normalizedKind = owner_->normalizedDirectiveToken(owner_->blockDetailsSelectedKind_);
    const TherionHelpEntry commandHelpEntry = owner_->helpEntries_.value(normalizedKind);
    const QString commandHelpHtml = ContextHelpController::renderHelpHtml(normalizedKind,
                                                                           commandHelpEntry.summary,
                                                                           commandHelpEntry.syntax,
                                                                           commandHelpEntry.arguments,
                                                                           commandHelpEntry.acceptedValues,
                                                                           commandHelpEntry.options,
                                                                           false);

    auto buildIdHelpHtml = [owner = owner_, commandHelpEntry]() {
        QString idArgumentLine;
        for (const QString &argumentLine : commandHelpEntry.arguments) {
            if (argumentLine.contains(QStringLiteral("<id>"), Qt::CaseInsensitive)) {
                idArgumentLine = argumentLine.trimmed();
                break;
            }
        }
        if (idArgumentLine.isEmpty()) {
            for (const QString &argumentLine : commandHelpEntry.arguments) {
                if (owner->isRequiredArgumentSignatureForBlocks(argumentLine.section(QLatin1Char('='), 0, 0).trimmed())) {
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
        return html.join(QString());
    };

    if ((owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::StructuredOptions
         || owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::SimpleValue
         || owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::DataHeader)
        && owner_->blockDetailsIdEdit_ != nullptr
        && owner_->blockDetailsIdEdit_->hasFocus()) {
        const bool idSemantics = owner_->blockDetailsPrimaryFieldLabel_ != nullptr
            && owner_->blockDetailsPrimaryFieldLabel_->text().trimmed().compare(TextEditorTab::tr("ID"), Qt::CaseInsensitive) == 0;
        if (owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::SimpleValue) {
            owner_->blockDetailsHelpBrowser_->setHtml(commandHelpHtml);
            return;
        }
        if (owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::DataHeader) {
            const QString html = QStringLiteral("<p><b>Style:</b> first token after <code>data</code> (for example <code>normal</code>).</p>"
                                                "<p><b>Readings order:</b> remaining tokens that define row column order (for example <code>from to length compass clino</code>).</p>"
                                                "<p>Use `Edit Data Rows...` to modify measurement rows and directives.</p>%1")
                                     .arg(commandHelpHtml);
            owner_->blockDetailsHelpBrowser_->setHtml(html);
            return;
        }
        if (!idSemantics) {
            owner_->blockDetailsHelpBrowser_->setHtml(commandHelpHtml);
            return;
        }
        const QString idHelpHtml = buildIdHelpHtml();
        owner_->blockDetailsHelpBrowser_->setHtml(!idHelpHtml.isEmpty() ? idHelpHtml : commandHelpHtml);
        return;
    }

    if (owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::StructuredOptions
        && owner_->blockDetailsAdditionalPositionalEdit_ != nullptr
        && owner_->blockDetailsAdditionalPositionalEdit_->hasFocus()) {
        const QString html = QStringLiteral("<p><b>Extra arguments</b> keep unsupported positional tokens intact.</p>"
                                            "<p>Prefer explicit key/value options where available.</p>%1")
                                 .arg(commandHelpHtml);
        owner_->blockDetailsHelpBrowser_->setHtml(html);
        return;
    }
    if (owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::DataHeader
        && owner_->blockDetailsReadingsTagEditorHasEditorFocus()) {
        const QString html = QStringLiteral("<p><b>Style:</b> first token after <code>data</code> (for example <code>normal</code>).</p>"
                                            "<p><b>Readings order:</b> tokenized column order. Add a token and confirm with Enter/Space; remove via chip <code>✕</code>.</p>"
                                            "<p>Use `Edit Data Rows...` to modify measurement rows and directives.</p>%1")
                                 .arg(commandHelpHtml);
        owner_->blockDetailsHelpBrowser_->setHtml(html);
        return;
    }
    if ((owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::StructuredOptions
         || owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::SimpleValue
         || owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::DataHeader)
        && owner_->blockDetailsCommentEdit_ != nullptr
        && owner_->blockDetailsCommentEdit_->hasFocus()) {
        const QString markerText = QString(owner_->blockDetailsCommentMarker_);
        const QString html = QStringLiteral("<p><b>Inline comment:</b> optional end-of-line note appended as <code>%1 ...</code>.</p>%2")
                                 .arg(markerText.toHtmlEscaped(), commandHelpHtml);
        owner_->blockDetailsHelpBrowser_->setHtml(html);
        return;
    }

    owner_->blockDetailsHelpBrowser_->setHtml(commandHelpHtml);
}
}
