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

bool MapEditorTab::applyValidationFixes(const QVector<TherionSourceDiagnosticFix> &fixes)
{
    if (textEditor_ == nullptr) {
        return false;
    }
    return textEditor_->applyValidationFixes(fixes);
}
}
