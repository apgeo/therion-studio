#include "ContextHelpController.h"

#include <QCoreApplication>

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
        html += QStringLiteral("<p><strong>%1:</strong> %2</p>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Summary"),
                         summary.toHtmlEscaped());
    }
    if (includeSyntax && !syntax.isEmpty()) {
        html += QStringLiteral("<p><strong>%1:</strong> <code>%2</code></p>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Syntax"),
                         syntax.toHtmlEscaped());
    }
    if (!arguments.isEmpty()) {
        html += QStringLiteral("<h4>%1</h4>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Arguments"));
        html += renderList(arguments);
    }
    if (!acceptedValues.isEmpty()) {
        html += QStringLiteral("<h4>%1</h4>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Accepted Values"));
        html += renderList(acceptedValues);
    }
    if (!options.isEmpty()) {
        html += QStringLiteral("<h4>%1</h4>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Options"));
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
        html += QStringLiteral("<p><strong>%1:</strong> %2</p>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Summary"),
                         summary.toHtmlEscaped());
    } else {
        html += QStringLiteral("<p><strong>%1:</strong> %2</p>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Summary"),
                         noSummaryFallback.toHtmlEscaped());
    }
    return html;
}

QString ContextHelpController::renderValidationHtml(const QString &cursorToken,
                                                    const QString &detailMessage,
                                                    const QStringList &allowedValues)
{
    QString html;
    html += QStringLiteral("<h3>%1</h3>")
                .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Validation"));
    html += QStringLiteral("<p><strong>%1:</strong> <code>%2</code></p>")
                .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Token"),
                     cursorToken.toHtmlEscaped());
    if (!detailMessage.isEmpty()) {
        html += QStringLiteral("<p>%1</p>").arg(detailMessage);
    }
    html += QStringLiteral("<h4>%1</h4>")
                .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Allowed Values"));
    html += renderList(allowedValues);
    return html;
}
}
