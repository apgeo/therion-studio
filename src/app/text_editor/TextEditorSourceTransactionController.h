#pragma once

#include <QString>

#include <functional>

class QUndoStack;

namespace TherionStudio
{
class TextEditorTab;

struct TextEditorSourceTransactionContext
{
    TextEditorTab *textEditor = nullptr;
    QUndoStack *undoStack = nullptr;
    bool *commandApplyInProgress = nullptr;
    std::function<void()> flushPendingRefresh;
    std::function<void(const QString &)> statusCallback;
};

struct TextEditorSourceTransactionRequest
{
    QString label;
    QString beforeText;
    QString afterText;
    QString undoStatusMessage;
    QString redoStatusMessage;
};

class TextEditorSourceTransactionController final
{
public:
    explicit TextEditorSourceTransactionController(TextEditorSourceTransactionContext context);

    void recordSnapshot(const TextEditorSourceTransactionRequest &request);
    void applyChangeWithSnapshot(const TextEditorSourceTransactionRequest &request);

private:
    TextEditorSourceTransactionContext context_;
};
}
