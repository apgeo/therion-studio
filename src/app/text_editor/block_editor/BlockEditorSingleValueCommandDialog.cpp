#include "BlockEditorSingleValueCommandDialog.h"

#include <QCoreApplication>

#include "BlockEditorDirectiveRules.h"
#include "../ContextHelpController.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "../../../core/TherionDocumentParser.h"

#include <utility>

namespace TherionStudio
{
BlockEditorSingleValueCommandDialog::BlockEditorSingleValueCommandDialog(BlockEditorSingleValueCommandDialogContext context)
    : context_(std::move(context))
{
}

QString BlockEditorSingleValueCommandDialog::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorSingleValueCommandDialog", text);
}

std::optional<QString> BlockEditorSingleValueCommandDialog::configureLine(
    const QString &commandName,
    const QString &sourceLine,
    int lineNumber)
{
    if (context_.commandMetadata == nullptr || lineNumber <= 0) {
        return std::nullopt;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(sourceLine, lineNumber);
    if (parsedLine.tokens.isEmpty()) {
        return std::nullopt;
    }

    const QString currentValue = parsedLine.tokens.size() > 1
        ? parsedLine.tokens.mid(1).join(QLatin1Char(' '))
        : QString();

    QDialog dialog(context_.dialogParent);
    dialog.setWindowTitle(tr("Configure Block"));
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto *formLayout = new QFormLayout;
    auto *valueEdit = new QLineEdit(currentValue, &dialog);
    formLayout->addRow(tr("Value"), valueEdit);
    layout->addLayout(formLayout);

    TherionHelpEntry helpEntry = context_.commandMetadata->helpEntries.value(commandName);
    if (helpEntry.summary.trimmed().isEmpty()
        && helpEntry.syntax.trimmed().isEmpty()
        && helpEntry.arguments.isEmpty()
        && helpEntry.options.isEmpty()
        && helpEntry.acceptedValues.isEmpty()) {
        const QString openingDirective = BlockEditorDirectiveRules::completionOpeningDirectiveForClosing(commandName);
        if (!openingDirective.isEmpty()) {
            helpEntry = context_.commandMetadata->helpEntries.value(openingDirective);
        }
    }

    auto *helpLabel = new QLabel(tr("Contextual Help"), &dialog);
    QFont helpLabelFont = helpLabel->font();
    helpLabelFont.setBold(true);
    helpLabel->setFont(helpLabelFont);
    layout->addWidget(helpLabel);

    auto *helpBrowser = new QTextBrowser(&dialog);
    helpBrowser->setOpenLinks(false);
    helpBrowser->setOpenExternalLinks(false);
    helpBrowser->setMinimumHeight(140);
    helpBrowser->setHtml(ContextHelpController::renderHelpHtml(commandName,
                                                               helpEntry.summary,
                                                               helpEntry.syntax,
                                                               helpEntry.arguments,
                                                               helpEntry.acceptedValues,
                                                               helpEntry.options,
                                                               true));
    layout->addWidget(helpBrowser, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const QString updatedValue = valueEdit->text();
    if (updatedValue.trimmed().isEmpty()) {
        QMessageBox::warning(context_.dialogParent, tr("Configure Block"), tr("Value cannot be empty."));
        return std::nullopt;
    }

    QString updatedLine = QStringLiteral("%1 %2").arg(commandName, updatedValue.trimmed());
    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto match = indentPattern.match(sourceLine);
    if (match.hasMatch()) {
        updatedLine.prepend(match.captured(0));
    }

    return updatedLine;
}
}
