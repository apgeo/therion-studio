#include "BlockEditorConfigureController.h"

#include <QCoreApplication>

#include "BlockEditorDataBlockDialog.h"
#include "BlockEditorSourceController.h"
#include "BlockEditorSourceText.h"
#include "../../../core/TherionDocumentEditor.h"

#include <QMessageBox>

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
    return QCoreApplication::translate("TherionStudio::BlockEditorConfigureController", text);
}

void BlockEditorConfigureController::configureBlock(const QString &kind, int lineNumber, bool showCommandHelpOnly)
{
    if (context_.sourceContext == nullptr || lineNumber <= 0) {
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

    const QString normalizedKind = kind.toLower();
    Q_UNUSED(showCommandHelpOnly);
    if (normalizedKind != QStringLiteral("data")) {
        return;
    }

    BlockEditorDataBlockDialog dataDialog(context_.dataBlockDialogContext);
    const std::optional<BlockEditorDataBlockDialogResult> dataResult = dataDialog.configureRows(lines, lineNumber);
    if (!dataResult.has_value()) {
        return;
    }

    const QString nextContents = source.text();
    const QStringList nextLines = blockEditorNormalizedSourceLines(nextContents);
    if (lineNumber <= 0 || lineNumber > nextLines.size()) {
        return;
    }

    TherionSourceTextEdit sourceEdit;
    if (!blockEditorSourceLineRangeReplacementEdit(nextContents,
                                                   lineNumber + 1,
                                                   dataResult->dataBodyLastLine,
                                                   dataResult->rowLines,
                                                   &sourceEdit)) {
        return;
    }
    if (!source.applyTextEdit(sourceEdit)) {
        return;
    }

    if (dataResult->schemaMismatchDetected) {
        QMessageBox::information(context_.dialogParent,
                                 tr("Schema no longer matches"),
                                 tr("Some existing data rows did not match the current columns schema and were preserved as best effort. Please review rows in Raw mode if needed."));
    }
}
}
