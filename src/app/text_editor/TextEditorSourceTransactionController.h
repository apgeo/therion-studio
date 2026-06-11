#pragma once

#include "../../core/TherionDocumentEditor.h"

#include <QString>
#include <QVector>

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
    QVector<TherionSourceTextEdit> sourceEdits;
    QString undoStatusMessage;
    QString redoStatusMessage;
};

void applyTextEditorSourceSnapshot(TextEditorTab *textEditor, const QString &contents);

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
