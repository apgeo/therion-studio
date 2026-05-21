#pragma once

namespace TherionStudio
{
class TextEditorTab;

class TextEditorCursorController final
{
public:
    explicit TextEditorCursorController(TextEditorTab *owner);

    void goToLine(int lineNumber);
    void goToLineColumn(int lineNumber, int columnNumber);
    int currentLineNumber() const;
    int currentColumnNumber() const;
    void handleCursorPositionChanged();
    void refreshCurrentLineHighlight();

private:
    TextEditorTab *owner_ = nullptr;
};
}
