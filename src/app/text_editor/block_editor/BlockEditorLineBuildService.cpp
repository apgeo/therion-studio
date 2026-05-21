#include "BlockEditorLineBuildService.h"

#include "BlockEditorSourceController.h"
#include "../TextEditorOptionValidation.h"
#include "../TextEditorTab.h"

#include "../../../core/TherionCommandSyntax.h"
#include "../../../core/TherionDocumentParser.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace TherionStudio
{
BlockEditorLineBuildService::BlockEditorLineBuildService(const TextEditorTab *owner)
    : owner_(owner)
{
}

bool BlockEditorLineBuildService::buildUpdatedLine(QString *updatedLine, QString *validationError) const
{
    if (owner_ == nullptr || updatedLine == nullptr) {
        return false;
    }
    updatedLine->clear();
    if (validationError != nullptr) {
        validationError->clear();
    }

    if (owner_->blockDetailsSelectedLineNumber_ <= 0
        || owner_->blockDetailsSelectedKind_.isEmpty()
        || !BlockEditorSourceController(owner_).hasEditor()) {
        if (validationError != nullptr) {
            *validationError = TextEditorTab::tr("No block is selected.");
        }
        return false;
    }
    const BlockEditorSourceController source(owner_);

    QStringList lines = source.normalizedLines();
    if (owner_->blockDetailsSelectedLineNumber_ > lines.size()) {
        if (validationError != nullptr) {
            *validationError = TextEditorTab::tr("Selected line is out of range.");
        }
        return false;
    }

    const QString normalizedKind = owner_->normalizedDirectiveToken(owner_->blockDetailsSelectedKind_);
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(owner_->blockDetailsSelectedLineNumber_ - 1),
                                                                           owner_->blockDetailsSelectedLineNumber_);
    if (parsedLine.tokens.isEmpty()) {
        if (validationError != nullptr) {
            *validationError = TextEditorTab::tr("Selected line is empty.");
        }
        return false;
    }

    const QRegularExpression indentPattern(QStringLiteral(R"(^[ \t]*)"));
    const auto match = indentPattern.match(lines.at(owner_->blockDetailsSelectedLineNumber_ - 1));
    const QString indent = match.hasMatch() ? match.captured(0) : QString();
    const QString commandToken = parsedLine.tokens.value(0).trimmed().isEmpty()
        ? normalizedKind
        : parsedLine.tokens.value(0).trimmed();

    QString result = QStringLiteral("%1%2").arg(indent, commandToken);

    if (owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::SimpleValue) {
        const QString updatedValue = owner_->blockDetailsIdEdit_ != nullptr
            ? owner_->blockDetailsIdEdit_->text().trimmed()
            : QString();
        if (updatedValue.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = TextEditorTab::tr("Value cannot be empty.");
            }
            return false;
        }
        if (owner_->commandPrimaryValueIsPerson_.value(normalizedKind, false)) {
            result += QStringLiteral(" ") + serializeTherionArgumentToken(updatedValue);
        } else {
            result += QStringLiteral(" ") + updatedValue;
        }
        if (owner_->blockDetailsAdditionalPositionalEdit_ != nullptr
            && owner_->blockDetailsAdditionalPositionalEdit_->isVisible()) {
            const QString secondaryValue = owner_->blockDetailsAdditionalPositionalEdit_->text().trimmed();
            if (!secondaryValue.isEmpty()) {
                result += QStringLiteral(" ") + secondaryValue;
            }
        }
    } else if (owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::DataHeader) {
        const QString updatedStyle = owner_->blockDetailsIdEdit_ != nullptr
            ? owner_->blockDetailsIdEdit_->text().trimmed()
            : QString();
        if (updatedStyle.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = TextEditorTab::tr("Style cannot be empty.");
            }
            return false;
        }
        const QString updatedReadingsOrder = owner_->blockDetailsReadingsOrderTextForBuild().trimmed();
        if (updatedReadingsOrder.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = TextEditorTab::tr("Readings order cannot be empty.");
            }
            return false;
        }
        result += QStringLiteral(" ") + updatedStyle;
        result += QStringLiteral(" ") + updatedReadingsOrder;
    } else if (owner_->blockDetailsMode_ == TextEditorTab::BlockDetailsMode::StructuredOptions) {
        const bool requiresId = owner_->commandHasRequiredIdArgument(normalizedKind);
        const QString updatedId = owner_->blockDetailsIdEdit_ != nullptr
            ? owner_->blockDetailsIdEdit_->text().trimmed()
            : QString();
        if (requiresId && updatedId.isEmpty()) {
            if (validationError != nullptr) {
                *validationError = TextEditorTab::tr("ID cannot be empty.");
            }
            return false;
        }

        QStringList serializedOptions;
        if (owner_->blockDetailsOptionsTable_ != nullptr) {
            QVector<TextEditorOptionRow> optionRows;
            optionRows.reserve(owner_->blockDetailsOptionsTable_->rowCount());
            for (int row = 0; row < owner_->blockDetailsOptionsTable_->rowCount(); ++row) {
                const QString key = (owner_->blockDetailsOptionsTable_->item(row, 0) != nullptr
                                         ? owner_->blockDetailsOptionsTable_->item(row, 0)->text()
                                         : QString())
                                        .trimmed();
                const QString value = (owner_->blockDetailsOptionsTable_->item(row, 1) != nullptr
                                           ? owner_->blockDetailsOptionsTable_->item(row, 1)->text()
                                           : QString())
                                          .trimmed();
                optionRows.append(TextEditorOptionRow{key, value, 0});
            }

            const TextEditorOptionValidationResult validation = validateAndSerializeCommandOptions(
                commandToken,
                optionRows,
                owner_->commandOptionValueArityTokens_,
                owner_->commandOptionFixedArityByKey_,
                owner_->commandOptionArgumentLabelsByKey_,
                owner_->commandOptionValueTokens_,
                true);
            if (!validation.ok) {
                if (owner_->blockDetailsOptionsTable_ != nullptr
                    && validation.failingRow >= 0
                    && validation.failingRow < owner_->blockDetailsOptionsTable_->rowCount()) {
                    owner_->blockDetailsOptionsTable_->setCurrentCell(validation.failingRow, 0);
                }
                if (validationError != nullptr) {
                    *validationError = validation.errorMessage;
                }
                return false;
            }
            serializedOptions = validation.serializedOptions;
        }

        if (!updatedId.isEmpty()) {
            result += QStringLiteral(" ") + updatedId;
        }
        const QString updatedAdditionalPositionalTokens = owner_->blockDetailsAdditionalPositionalEdit_ != nullptr
            ? owner_->blockDetailsAdditionalPositionalEdit_->text().trimmed()
            : QString();
        if (!updatedAdditionalPositionalTokens.isEmpty()) {
            result += QStringLiteral(" ") + updatedAdditionalPositionalTokens;
        }
        if (!serializedOptions.isEmpty()) {
            result += QStringLiteral(" ") + serializedOptions.join(QLatin1Char(' '));
        }
    } else {
        if (validationError != nullptr) {
            *validationError = TextEditorTab::tr("This block cannot be edited in details pane.");
        }
        return false;
    }

    const QString updatedComment = owner_->blockDetailsCommentEdit_ != nullptr
        ? owner_->blockDetailsCommentEdit_->text().trimmed()
        : QString();
    if (!updatedComment.isEmpty()) {
        QChar marker = owner_->blockDetailsCommentMarker_;
        if (marker != QLatin1Char('#') && marker != QLatin1Char('%')) {
            marker = QLatin1Char('#');
        }
        result += QStringLiteral(" %1 %2").arg(QString(marker), updatedComment);
    }

    *updatedLine = result;
    return true;
}
}
