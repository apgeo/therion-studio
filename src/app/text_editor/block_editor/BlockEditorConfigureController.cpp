#include "BlockEditorConfigureController.h"

#include "BlockEditorCommandOptionsDialog.h"
#include "BlockEditorDataBlockDialog.h"
#include "BlockEditorDirectiveRules.h"
#include "BlockEditorSingleValueCommandDialog.h"
#include "BlockEditorSourceController.h"
#include "BlockEditorSourceText.h"
#include "../../../core/TherionDocumentParser.h"

#include <QMessageBox>
#include <QPlainTextEdit>

#include <optional>
#include <utility>

namespace TherionStudio
{
BlockEditorConfigureController::BlockEditorConfigureController(BlockEditorConfigureContext context)
    : context_(std::move(context))
{
}

QString BlockEditorConfigureController::tr(const char *text) const
{
    return context_.translate != nullptr ? context_.translate(text) : QString::fromUtf8(text);
}

void BlockEditorConfigureController::configureBlock(const QString &kind, int lineNumber, bool showCommandHelpOnly)
{
    if (context_.sourceContext == nullptr || context_.commandMetadata == nullptr || lineNumber <= 0) {
        return;
    }

    const BlockEditorSourceController source(context_.sourceContext());
    if (!source.hasEditor()) {
        return;
    }

    const QString contents = source.text();
    QStringList lines = blockEditorNormalizedSourceLines(contents);
    if (lineNumber > lines.size()) {
        return;
    }

    BlockEditorLogicalLine logicalLine;
    if (!blockEditorResolveLogicalLineAtLine(lines, lineNumber, &logicalLine)) {
        return;
    }

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(logicalLine.text, logicalLine.startLine);
    if (parsedLine.tokens.isEmpty()) {
        return;
    }

    const QString normalizedKind = kind.toLower();

    if (normalizedKind != QStringLiteral("data")) {
        if (!context_.commandMetadata->commandOptionTokens.value(normalizedKind).isEmpty()
            || (context_.commandSupportsInlineIdField != nullptr && context_.commandSupportsInlineIdField(normalizedKind))) {
            const BlockEditorCommandIdFieldMode idFieldMode = context_.commandHasRequiredIdArgument != nullptr
                    && context_.commandHasRequiredIdArgument(normalizedKind)
                ? BlockEditorCommandIdFieldMode::Required
                : (context_.commandSupportsInlineIdField != nullptr && context_.commandSupportsInlineIdField(normalizedKind)
                       ? BlockEditorCommandIdFieldMode::Optional
                       : BlockEditorCommandIdFieldMode::None);
            BlockEditorCommandOptionsDialog optionsDialog(context_.commandOptionsDialogContext);
            const std::optional<QString> updatedLine =
                optionsDialog.configureLine(normalizedKind,
                                            logicalLine.text,
                                            logicalLine.startLine,
                                            idFieldMode,
                                            showCommandHelpOnly
                                                ? BlockEditorCommandOptionsHelpMode::CommandOnly
                                                : BlockEditorCommandOptionsHelpMode::SelectedRow);
            if (!updatedLine.has_value()) {
                return;
            }
            if (!blockEditorReplaceSourceLineRange(&lines,
                                                   logicalLine.startLine,
                                                   logicalLine.endLine,
                                                   QStringList{updatedLine.value()})) {
                return;
            }
            source.replaceWithLines(contents, lines);
            return;
        }
    }

    if (normalizedKind == QStringLiteral("data")) {
        BlockEditorDataBlockDialog dataDialog(context_.dataBlockDialogContext);
        const std::optional<BlockEditorDataBlockDialogResult> dataResult = dataDialog.configureRows(lines, lineNumber);
        if (!dataResult.has_value()) {
            return;
        }

        const QString nextContents = source.text();
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

        source.replaceWithLines(nextContents, nextLines);

        if (dataResult->schemaMismatchDetected) {
            QMessageBox::information(context_.dialogParent,
                                     tr("Schema no longer matches"),
                                     tr("Some existing data rows did not match the current columns schema and were preserved as best effort. Please review rows in Raw mode if needed."));
        }
        return;
    }

    const bool hasCatalogOptions = !context_.commandMetadata->commandOptionTokens.value(normalizedKind).isEmpty();
    if (!BlockEditorDirectiveRules::isContainerBlockDirective(normalizedKind)
        && normalizedKind != QStringLiteral("data")
        && !hasCatalogOptions) {
        BlockEditorSingleValueCommandDialog valueDialog(context_.singleValueCommandDialogContext);
        const std::optional<QString> updatedLine =
            valueDialog.configureLine(normalizedKind, logicalLine.text, logicalLine.startLine);
        if (!updatedLine.has_value()) {
            return;
        }
        if (!blockEditorReplaceSourceLineRange(&lines,
                                               logicalLine.startLine,
                                               logicalLine.endLine,
                                               QStringList{updatedLine.value()})) {
            return;
        }

        source.replaceWithLines(contents, lines);
        return;
    }

    QMessageBox::information(context_.dialogParent,
                             tr("Configure Block"),
                             tr("Configuration for `%1` is not implemented yet.")
                                 .arg(kind));
}
}
