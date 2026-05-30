#include "ContextHelpController.h"

#include <QCoreApplication>

namespace TherionStudio
{
namespace
{
QString renderSignatureList(const QStringList &items)
{
    if (items.isEmpty()) {
        return QString();
    }

    QString html = QStringLiteral("<ul style=\"margin-top:0; margin-bottom:8px; margin-left:12px; padding-left:12px; -qt-list-indent:0;\">");
    for (const QString &item : items) {
        const QString trimmed = item.trimmed();
        const int separatorIndex = trimmed.indexOf(QStringLiteral(" = "));
        const QString signature = separatorIndex >= 0 ? trimmed.left(separatorIndex).trimmed() : trimmed;
        const QString description = separatorIndex >= 0 ? trimmed.mid(separatorIndex + 3).trimmed() : QString();
        if (signature.isEmpty()) {
            html += QStringLiteral("<li style=\"margin-bottom:4px;\">%1</li>").arg(item.toHtmlEscaped());
            continue;
        }

        html += QStringLiteral("<li style=\"margin-bottom:4px;\"><b><tt>%1</tt></b>")
                    .arg(signature.toHtmlEscaped());
        if (!description.isEmpty()) {
            html += QStringLiteral(" %1").arg(description.toHtmlEscaped());
        }
        html += QStringLiteral("</li>");
    }
    html += QStringLiteral("</ul>");
    return html;
}
}

QString ContextHelpController::renderList(const QStringList &items)
{
    if (items.isEmpty()) {
        return QString();
    }

    QString html = QStringLiteral("<ul style=\"margin-top:0; margin-bottom:8px; margin-left:12px; padding-left:12px; -qt-list-indent:0;\">");
    for (const QString &item : items) {
        html += QStringLiteral("<li style=\"margin-bottom:4px;\">%1</li>").arg(item.toHtmlEscaped());
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
        html += renderSignatureList(arguments);
    }
    if (!acceptedValues.isEmpty()) {
        html += QStringLiteral("<h4>%1</h4>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Accepted Values"));
        html += renderList(acceptedValues);
    }
    if (!options.isEmpty()) {
        html += QStringLiteral("<h4>%1</h4>")
                    .arg(QCoreApplication::translate("TherionStudio::ContextHelpController", "Options"));
        html += renderSignatureList(options);
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
