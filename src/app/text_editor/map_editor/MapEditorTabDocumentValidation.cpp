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
    Q_UNUSED(diagnostics)
    // Keep project diagnostics in the Validation panel for map-editor tabs.
    // Injecting external diagnostics into the embedded text editor can re-enter
    // syntax highlighting while the visual scene is handling drag/edit gestures.
}

bool MapEditorTab::applyValidationFixes(const QVector<TherionSourceDiagnosticFix> &fixes)
{
    if (textEditor_ == nullptr) {
        return false;
    }
    return textEditor_->applyValidationFixes(fixes);
}
}
