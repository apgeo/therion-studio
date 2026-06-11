#pragma once

#include <functional>

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

#include "../../core/TherionDocumentEditor.h"

class QPlainTextEdit;

namespace TherionStudio
{
struct TextEditorSourceRewriteContext
{
    QPlainTextEdit *editor = nullptr;
    bool *loading = nullptr;
    int *currentLineNumber = nullptr;
    std::function<void()> applyDirtyStateFromCurrentState;
    std::function<void()> rebuildBlocksCanvasFromText;
    std::function<void()> documentTextChanged;
    std::function<void()> updateContextHelp;
};

class TextEditorSourceRewriteController final
{
public:
    explicit TextEditorSourceRewriteController(TextEditorSourceRewriteContext context);

    bool rewriteStructureEntryName(int lineNumber,
                                   const QString &category,
                                   const QString &newName,
                                   QString *errorMessage = nullptr);
    void applySourceTextEditsForCommandWithUndo(QVector<TherionSourceTextEdit> edits);
    void replaceTextForCommand(const QString &contents);
    void replaceTextForCommandWithUndo(const QString &contents);
    void replaceTextForSystemNormalization(const QString &contents);

private:
    void replaceEditorText(const QString &contents, bool recordUndoStep);
    void replaceTextPreservingCursor(const QString &contents,
                                     bool emitDocumentTextChanged,
                                     bool rebuildBlocksCanvas,
                                     bool applyDirtyState,
                                     bool recordUndoStep);
    void applyTextEditsPreservingCursor(QVector<TherionSourceTextEdit> edits,
                                        bool emitDocumentTextChanged,
                                        bool rebuildBlocksCanvas,
                                        bool applyDirtyState,
                                        bool recordUndoStep);
    void replaceTextSelectingLine(const QString &contents, int lineNumber, bool recordUndoStep);

    TextEditorSourceRewriteContext context_;
};
}
