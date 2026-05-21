#include "BlockEditorConfigureController.h"

#include "BlockEditorCommandOptionsDialog.h"
#include "BlockEditorDataBlockDialog.h"
#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSingleValueCommandDialog.h"
#include "BlockEditorSourceController.h"
#include "BlockEditorSourceText.h"
#include "../TextEditorTab.h"
#include "../../../core/TherionDocumentParser.h"

#include <QMessageBox>
#include <QPlainTextEdit>

#include <optional>

namespace TherionStudio
{
BlockEditorConfigureController::BlockEditorConfigureController(TextEditorTab *owner)
    : owner_(owner)
{
}

void BlockEditorConfigureController::configureBlock(const QString &kind, int lineNumber)
{
    if (owner_ == nullptr || owner_->editor_ == nullptr || lineNumber <= 0) {
        return;
    }

    const QString contents = owner_->editor_->toPlainText();
    QStringList lines = blockEditorNormalizedSourceLines(contents);
    if (lineNumber > lines.size()) {
        return;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineNumber - 1), lineNumber);
    if (parsedLine.tokens.isEmpty()) {
        return;
    }

    const QString normalizedKind = kind.toLower();

    if (normalizedKind != QStringLiteral("data")) {
        if (!owner_->commandMetadata().commandOptionTokens.value(normalizedKind).isEmpty()
            || owner_->commandSupportsInlineIdField(normalizedKind)) {
            const BlockEditorCommandIdFieldMode idFieldMode = owner_->commandHasRequiredIdArgument(normalizedKind)
                ? BlockEditorCommandIdFieldMode::Required
                : (owner_->commandSupportsInlineIdField(normalizedKind)
                       ? BlockEditorCommandIdFieldMode::Optional
                       : BlockEditorCommandIdFieldMode::None);
            BlockEditorCommandOptionsDialog optionsDialog(owner_);
            const std::optional<QString> updatedLine =
                optionsDialog.configureLine(normalizedKind, lines.at(lineNumber - 1), lineNumber, idFieldMode);
            if (!updatedLine.has_value()) {
                return;
            }
            lines[lineNumber - 1] = updatedLine.value();
            BlockEditorSourceController(owner_).replaceWithLines(contents, lines);
            return;
        }
    }

    if (normalizedKind == QStringLiteral("data")) {
        BlockEditorDataBlockDialog dataDialog(owner_);
        const std::optional<BlockEditorDataBlockDialogResult> dataResult = dataDialog.configureRows(lines, lineNumber);
        if (!dataResult.has_value()) {
            return;
        }

        const QString nextContents = owner_->editor_->toPlainText();
        QStringList nextLines = blockEditorNormalizedSourceLines(nextContents);
        if (lineNumber <= 0 || lineNumber > nextLines.size()) {
            return;
        }

        if (!blockEditorReplaceSourceLineRange(&nextLines,
                                               lineNumber + 1,
                                               dataResult->dataBodyLastLine,
                                               dataResult->rowLines)) {
            return;
        }

        BlockEditorSourceController(owner_).replaceWithLines(nextContents, nextLines);

        if (dataResult->schemaMismatchDetected) {
            QMessageBox::information(owner_,
                                     TextEditorTab::tr("Schema no longer matches"),
                                     TextEditorTab::tr("Some existing data rows did not match the current columns schema and were preserved as best effort. Please review rows in Raw mode if needed."));
        }
        return;
    }

    const bool hasCatalogOptions = !owner_->commandMetadata().commandOptionTokens.value(normalizedKind).isEmpty();
    if (!BlockEditorDirectiveRules::isContainerBlockDirective(normalizedKind)
        && normalizedKind != QStringLiteral("data")
        && !hasCatalogOptions) {
        BlockEditorSingleValueCommandDialog valueDialog(owner_);
        const std::optional<QString> updatedLine =
            valueDialog.configureLine(normalizedKind, lines.at(lineNumber - 1), lineNumber);
        if (!updatedLine.has_value()) {
            return;
        }
        lines[lineNumber - 1] = updatedLine.value();

        BlockEditorSourceController(owner_).replaceWithLines(contents, lines);
        return;
    }

    QMessageBox::information(owner_,
                             TextEditorTab::tr("Configure Block"),
                             TextEditorTab::tr("Configuration for `%1` is not implemented yet.")
                                 .arg(kind));
}
}
