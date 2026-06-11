#include "TextEditorTab.h"

#include "TextEditorSourceRewriteController.h"
#include "TextEditorValidationCatalog.h"
#include "../../core/TherionDocumentEditor.h"
#include "../../core/TherionFileTypes.h"
#include "../../core/TherionSourceValidator.h"

#include <QPlainTextEdit>

#include <utility>

namespace TherionStudio
{
TherionSourceValidationResult TextEditorTab::validateDocument() const
{
    if (editor_ == nullptr) {
        return {};
    }

    TherionSourceValidationResult result =
        TherionSourceValidator::validate(editor_->toPlainText(), validationCatalogFromCommandMetadata(commandMetadata_));
    if (!isTherionConfigFilePath(filePath_)) {
        return result;
    }

    QVector<TherionSourceDiagnostic> filteredDiagnostics;
    filteredDiagnostics.reserve(result.diagnostics.size());
    for (const TherionSourceDiagnostic &diagnostic : std::as_const(result.diagnostics)) {
        if (diagnostic.code != QStringLiteral("unknown-command")) {
            filteredDiagnostics.append(diagnostic);
        }
    }
    result.diagnostics = filteredDiagnostics;
    return result;
}

bool TextEditorTab::applyValidationFixes(const QVector<TherionSourceDiagnosticFix> &fixes)
{
    if (editor_ == nullptr || sourceRewriteController_ == nullptr || fixes.isEmpty()) {
        return false;
    }

    const QString beforeText = editor_->toPlainText();
    const QVector<TherionSourceTextEdit> edits = TherionSourceValidator::validationFixEdits(beforeText, fixes);
    if (edits.isEmpty()) {
        return false;
    }

    QString afterText = beforeText;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, edits)) {
        return false;
    }
    if (afterText == beforeText) {
        return false;
    }

    sourceRewriteController_->applySourceTextEditsForCommandWithUndo(edits);
    return true;
}
}
