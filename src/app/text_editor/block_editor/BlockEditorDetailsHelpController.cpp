#include "BlockEditorDetailsHelpController.h"

#include <QCoreApplication>

#include "BlockEditorDirectiveRules.h"
#include "../ContextHelpController.h"
#include "../ContextHelpInspector.h"
#include "../TextEditorCommandMetadata.h"

#include <QLabel>
#include <QLineEdit>
#include <QTextBrowser>

#include <utility>

namespace TherionStudio
{
BlockEditorDetailsHelpController::BlockEditorDetailsHelpController(BlockEditorDetailsHelpContext context)
    : context_(std::move(context))
{
}

QString BlockEditorDetailsHelpController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorDetailsHelpController", text);
}

bool BlockEditorDetailsHelpController::hasRequiredContext() const
{
    return context_.tearingDown != nullptr
        && context_.helpBrowser != nullptr
        && context_.helpInspector != nullptr
        && context_.commentMarker != nullptr
        && context_.commandMetadata != nullptr
        && context_.selectedKind
        && context_.normalizeDirectiveToken
        && context_.isRequiredArgumentSignature
        && context_.readingsTagEditorHasEditorFocus
        && context_.isStructuredOptionsMode
        && context_.isSimpleValueMode
        && context_.isDataHeaderMode;
}

bool BlockEditorDetailsHelpController::isDetailsModeWithEditableFields() const
{
    return context_.isStructuredOptionsMode()
        || context_.isSimpleValueMode()
        || context_.isDataHeaderMode();
}

QString BlockEditorDetailsHelpController::buildIdHelpHtml(const TherionHelpEntry &commandHelpEntry) const
{
    QString idArgumentLine;
    for (const QString &argumentLine : commandHelpEntry.arguments) {
        if (argumentLine.contains(QStringLiteral("<id>"), Qt::CaseInsensitive)) {
            idArgumentLine = argumentLine.trimmed();
            break;
        }
    }
    if (idArgumentLine.isEmpty()) {
        for (const QString &argumentLine : commandHelpEntry.arguments) {
            if (context_.isRequiredArgumentSignature(argumentLine.section(QLatin1Char('='), 0, 0).trimmed())) {
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
    html << tr("<p><b>Parameter:</b> %1</p>").arg(signature.isEmpty()
                                                     ? QStringLiteral("&lt;id&gt;")
                                                     : signature.toHtmlEscaped());
    if (!description.isEmpty()) {
        html << tr("<p><b>Description:</b> %1</p>").arg(description.toHtmlEscaped());
    }
    return html.join(QString());
}

void BlockEditorDetailsHelpController::updateHelpForCurrentFocus()
{
    if (!hasRequiredContext() || (*context_.tearingDown)) {
        return;
    }

    const QString selectedKind = context_.selectedKind();
    if (selectedKind.isEmpty()) {
        context_.helpInspector->setTitle(tr("Context Help"));
        context_.helpBrowser->setHtml(tr("<p>Select a block parameter to see contextual help.</p>"));
        return;
    }

    const QString normalizedKind = context_.normalizeDirectiveToken(selectedKind);
    if (BlockEditorDirectiveRules::isMapObjectReferenceKind(normalizedKind)) {
        context_.helpInspector->setTitle(tr("Object Reference"));
        context_.helpBrowser->setHtml(tr("<p>References an existing scrap or map from inside a <code>map</code> block. "
                                         "Edit the target to the referenced object name. Unresolved names are preserved so incomplete maps stay round-trip safe.</p>"));
        return;
    }
    if (BlockEditorDirectiveRules::isUnrecognizedKind(normalizedKind)) {
        context_.helpInspector->setTitle(tr("Unrecognized line"));
        context_.helpBrowser->setHtml(tr("<p>This source line could not be mapped to a known Block-editor command in the current context.</p>"
                                         "<p>Edit the full raw line directly. The source text is preserved exactly as entered.</p>"));
        return;
    }
    const TherionHelpEntry commandHelpEntry = context_.commandMetadata->helpEntries.value(normalizedKind);
    const QString commandHelpHtml = ContextHelpController::renderHelpHtml(normalizedKind,
                                                                           commandHelpEntry.summary,
                                                                           commandHelpEntry.syntax,
                                                                           commandHelpEntry.arguments,
                                                                           commandHelpEntry.acceptedValues,
                                                                           commandHelpEntry.options,
                                                                           true,
                                                                           false);
    context_.helpInspector->setTitle(normalizedKind);

    if (isDetailsModeWithEditableFields()
        && context_.idEdit != nullptr
        && context_.idEdit->hasFocus()) {
        const bool idSemantics = context_.primaryFieldLabel != nullptr
            && context_.primaryFieldLabel->text().trimmed().compare(tr("ID"), Qt::CaseInsensitive) == 0;
        if (context_.isSimpleValueMode()) {
            context_.helpBrowser->setHtml(commandHelpHtml);
            return;
        }
        if (context_.isDataHeaderMode()) {
            const QString html = tr("<p><b>Style:</b> first token after <code>data</code> (for example <code>normal</code>).</p>"
                                    "<p><b>Readings order:</b> remaining tokens that define row column order (for example <code>from to length compass clino</code>).</p>"
                                    "<p>Use <b>Edit Data Rows...</b> to modify measurement rows and directives.</p>%1")
                                     .arg(commandHelpHtml);
            context_.helpBrowser->setHtml(html);
            return;
        }
        if (!idSemantics) {
            context_.helpBrowser->setHtml(commandHelpHtml);
            return;
        }
        const QString idHelpHtml = buildIdHelpHtml(commandHelpEntry);
        context_.helpBrowser->setHtml(!idHelpHtml.isEmpty() ? idHelpHtml : commandHelpHtml);
        return;
    }

    if (context_.isStructuredOptionsMode()
        && context_.additionalPositionalEdit != nullptr
        && context_.additionalPositionalEdit->hasFocus()) {
        const QString html = tr("<p><b>Extra arguments</b> keep unsupported positional tokens intact.</p>"
                                "<p>Prefer explicit key/value options where available.</p>%1")
                                 .arg(commandHelpHtml);
        context_.helpBrowser->setHtml(html);
        return;
    }
    if (context_.isDataHeaderMode()
        && context_.readingsTagEditorHasEditorFocus()) {
        const QString html = tr("<p><b>Style:</b> first token after <code>data</code> (for example <code>normal</code>).</p>"
                                "<p><b>Readings order:</b> tokenized column order. Add a token and confirm with Enter/Space; remove via chip <code>✕</code>.</p>"
                                "<p>Use <b>Edit Data Rows...</b> to modify measurement rows and directives.</p>%1")
                                 .arg(commandHelpHtml);
        context_.helpBrowser->setHtml(html);
        return;
    }
    if (isDetailsModeWithEditableFields()
        && context_.commentEdit != nullptr
        && context_.commentEdit->hasFocus()) {
        const QString markerText = QString(*context_.commentMarker);
        const QString html = tr("<p><b>Inline comment:</b> optional end-of-line note appended as <code>%1 ...</code>.</p>%2")
                                 .arg(markerText.toHtmlEscaped(), commandHelpHtml);
        context_.helpBrowser->setHtml(html);
        return;
    }

    context_.helpBrowser->setHtml(commandHelpHtml);
}
}
