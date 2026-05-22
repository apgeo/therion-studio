#pragma once

#include <functional>

class QFrame;
class QLineEdit;
class QPlainTextEdit;

namespace TherionStudio
{
struct TextEditorCursorContext
{
    QPlainTextEdit *editor = nullptr;
    QFrame *searchBar = nullptr;
    QLineEdit *findEdit = nullptr;
    int *currentLineNumber = nullptr;
    int *currentColumnNumber = nullptr;
    int *highlightedLineNumber = nullptr;
    bool *loading = nullptr;
    bool *blocksModeActive = nullptr;
    std::function<void(bool)> setBlocksModeActive;
    std::function<void(int)> currentLineChanged;
    std::function<void(int, int)> cursorPositionChanged;
    std::function<void()> updateContextHelp;
    std::function<void()> updateValidationTooltipForCursor;
};

class TextEditorCursorController final
{
public:
    explicit TextEditorCursorController(TextEditorCursorContext context);

    void goToLine(int lineNumber);
    void goToLineColumn(int lineNumber, int columnNumber);
    int currentLineNumber() const;
    int currentColumnNumber() const;
    void handleCursorPositionChanged();
    void refreshCurrentLineHighlight();

private:
    void setHighlightedLineNumber(int lineNumber) const;

    TextEditorCursorContext context_;
};
}
