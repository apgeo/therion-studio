#include "ContextHelpController.h"

namespace TherionStudio
{
QString ContextHelpController::renderList(const QStringList &items)
{
    if (items.isEmpty()) {
        return QString();
    }

    QString html = QStringLiteral("<ul>");
    for (const QString &item : items) {
        html += QStringLiteral("<li>%1</li>").arg(item.toHtmlEscaped());
    }
    html += QStringLiteral("</ul>");
    return html;
}

QString ContextHelpController::renderHelpHtml(const QString &token,
                                              const QString &summary,
                                              const QString &syntax,
                                              const QStringList &arguments,
                                              const QStringList &acceptedValues,
                                              const QStringList &options,
                                              bool includeSyntax)
{
    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(token.toHtmlEscaped());

    if (!summary.isEmpty()) {
        html += QStringLiteral("<p><strong>Summary:</strong> %1</p>").arg(summary.toHtmlEscaped());
    }
    if (includeSyntax && !syntax.isEmpty()) {
        html += QStringLiteral("<p><strong>Syntax:</strong> <code>%1</code></p>").arg(syntax.toHtmlEscaped());
    }
    if (!arguments.isEmpty()) {
        html += QStringLiteral("<h4>Arguments</h4>");
        html += renderList(arguments);
    }
    if (!acceptedValues.isEmpty()) {
        html += QStringLiteral("<h4>Accepted Values</h4>");
        html += renderList(acceptedValues);
    }
    if (!options.isEmpty()) {
        html += QStringLiteral("<h4>Options</h4>");
        html += renderList(options);
    }

    return html;
}

QString ContextHelpController::renderHelpSummaryHtml(const QString &token,
                                                     const QString &summary,
                                                     const QString &noSummaryFallback)
{
    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(token.toHtmlEscaped());
    if (!summary.trimmed().isEmpty()) {
        html += QStringLiteral("<p><strong>Summary:</strong> %1</p>").arg(summary.toHtmlEscaped());
    } else {
        html += QStringLiteral("<p><strong>Summary:</strong> %1</p>").arg(noSummaryFallback.toHtmlEscaped());
    }
    return html;
}

QString ContextHelpController::renderValidationHtml(const QString &cursorToken,
                                                    const QString &detailMessage,
                                                    const QStringList &allowedValues)
{
    QString html;
    html += QStringLiteral("<h3>Validation</h3>");
    html += QStringLiteral("<p><strong>Token:</strong> <code>%1</code></p>").arg(cursorToken.toHtmlEscaped());
    if (!detailMessage.isEmpty()) {
        html += QStringLiteral("<p>%1</p>").arg(detailMessage);
    }
    html += QStringLiteral("<h4>Allowed Values</h4>");
    html += renderList(allowedValues);
    return html;
}
}
