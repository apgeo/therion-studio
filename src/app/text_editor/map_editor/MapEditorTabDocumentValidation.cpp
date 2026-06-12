#include "MapEditorTab.h"

#include "../TextEditorTab.h"

namespace TherionStudio
{
TherionSourceValidationResult MapEditorTab::validateDocument() const
{
    if (textEditor_ != nullptr) {
        return textEditor_->validateDocument();
    }
    return {};
}

void MapEditorTab::setProjectValidationDiagnostics(const QVector<TherionSourceDiagnostic> &diagnostics)
{
    if (textEditor_ != nullptr) {
        textEditor_->setProjectValidationDiagnostics(diagnostics);
    }
}

bool MapEditorTab::applyValidationFixes(const QVector<TherionSourceDiagnosticFix> &fixes)
{
    if (textEditor_ == nullptr) {
        return false;
    }
    return textEditor_->applyValidationFixes(fixes);
}
}
