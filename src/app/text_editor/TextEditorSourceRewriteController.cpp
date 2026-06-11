#include "TextEditorSourceRewriteController.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>

#include <algorithm>
#include <utility>

#include "../../core/TherionDocumentEditor.h"
#include "../../core/TherionBackgroundMetadata.h"

namespace TherionStudio
{
TextEditorSourceRewriteController::TextEditorSourceRewriteController(TextEditorSourceRewriteContext context)
    : context_(std::move(context))
{
}

bool TextEditorSourceRewriteController::rewriteStructureEntryName(int lineNumber,
                                                                  const QString &category,
                                                                  const QString &newName,
                                                                  QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::structureEntryNameRewriteEdits(contents, lineNumber, category, newName, &edits, errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, false, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::insertScrapBlock(const QString &preferredName,
                                                         int *insertedLineNumber,
                                                         QString *errorMessage,
                                                         const QString &options)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString beforeText = context_.editor->toPlainText();
    QString contents = beforeText;
    int resolvedLineNumber = 0;
    QVector<TherionSourceTextEdit> sourceEdits;
    if (!TherionDocumentEditor::appendScrapBlockEdits(beforeText,
                                                     preferredName,
                                                     &sourceEdits,
                                                     &resolvedLineNumber,
                                                     errorMessage,
                                                     options)
        || !TherionDocumentEditor::applySourceTextEdits(&contents, sourceEdits, errorMessage)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber, false);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::insertDraftGeometry(const QString &kind,
                                                            const QVector<QPointF> &vertices,
                                                            int *insertedLineNumber,
                                                            QString *errorMessage,
                                                            const TherionDraftObjectOptions &objectOptions,
                                                            const std::optional<QRectF> &initialAreaAdjustRect)
{
    if (context_.editor == nullptr) {
        return false;
    }

    QString contents = context_.editor->toPlainText();
    if (initialAreaAdjustRect.has_value()
        && initialAreaAdjustRect->isValid()
        && !parseTherionAreaAdjust(contents).valid) {
        contents = upsertTherionAreaAdjustMetadata(contents, *initialAreaAdjustRect);
    }
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftGeometry(&contents,
                                                    kind,
                                                    vertices,
                                                    &resolvedLineNumber,
                                                    errorMessage,
                                                    objectOptions)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber, false);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::insertDraftLineGeometry(const QStringList &coordinateRows,
                                                                int *insertedLineNumber,
                                                                QString *errorMessage,
                                                                const QString &lineOptions,
                                                                const TherionDraftObjectOptions &objectOptions,
                                                                const std::optional<QRectF> &initialAreaAdjustRect)
{
    if (context_.editor == nullptr) {
        return false;
    }

    QString contents = context_.editor->toPlainText();
    if (initialAreaAdjustRect.has_value()
        && initialAreaAdjustRect->isValid()
        && !parseTherionAreaAdjust(contents).valid) {
        contents = upsertTherionAreaAdjustMetadata(contents, *initialAreaAdjustRect);
    }
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftLineGeometry(&contents,
                                                        coordinateRows,
                                                        &resolvedLineNumber,
                                                        errorMessage,
                                                        lineOptions,
                                                        objectOptions)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber, false);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::insertDraftAreaGeometry(const QStringList &coordinateRows,
                                                                int *insertedLineNumber,
                                                                QString *errorMessage,
                                                                const TherionDraftObjectOptions &objectOptions,
                                                                const std::optional<QRectF> &initialAreaAdjustRect)
{
    if (context_.editor == nullptr) {
        return false;
    }

    QString contents = context_.editor->toPlainText();
    if (initialAreaAdjustRect.has_value()
        && initialAreaAdjustRect->isValid()
        && !parseTherionAreaAdjust(contents).valid) {
        contents = upsertTherionAreaAdjustMetadata(contents, *initialAreaAdjustRect);
    }
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftAreaGeometry(&contents,
                                                        coordinateRows,
                                                        &resolvedLineNumber,
                                                        errorMessage,
                                                        objectOptions)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber, false);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::rewritePointCoordinates(int lineNumber,
                                                                const QPointF &point,
                                                                QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::pointCoordinateRewriteEdits(contents, lineNumber, point, &edits, errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, true, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLineAreaVertex(int lineNumber,
                                                              const QString &kind,
                                                              int vertexIndex,
                                                              const QPointF &point,
                                                              QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::lineAreaVertexRewriteEdits(contents, lineNumber, kind, vertexIndex, point, &edits, errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, true, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLineOptionToggle(int lineNumber,
                                                                const QString &optionName,
                                                                bool enabled,
                                                                QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::lineOptionToggleRewriteEdits(contents, lineNumber, optionName, enabled, &edits, errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, true, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewritePointOrientation(int lineNumber,
                                                                bool enabled,
                                                                qreal orientationDegrees,
                                                                QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::pointOrientationRewriteEdits(contents,
                                                             lineNumber,
                                                             enabled,
                                                             orientationDegrees,
                                                             &edits,
                                                             errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, true, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLinePointOrientation(int lineNumber,
                                                                    int sourceVertexIndex,
                                                                    bool enabled,
                                                                    qreal orientationDegrees,
                                                                    QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::linePointOrientationRewriteEdits(contents,
                                                                 lineNumber,
                                                                 sourceVertexIndex,
                                                                 enabled,
                                                                 orientationDegrees,
                                                                 &edits,
                                                                 errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, true, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLinePointLeftSize(int lineNumber,
                                                                 int sourceVertexIndex,
                                                                 bool enabled,
                                                                 qreal sizeValue,
                                                                 QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::linePointLeftSizeRewriteEdits(contents,
                                                              lineNumber,
                                                              sourceVertexIndex,
                                                              enabled,
                                                              sizeValue,
                                                              &edits,
                                                              errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, true, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLineCoordinateRows(int lineNumber,
                                                                  const QStringList &coordinateRows,
                                                                  QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    const QString contents = context_.editor->toPlainText();
    QVector<TherionSourceTextEdit> edits;
    if (!TherionDocumentEditor::lineCoordinateRowsRewriteEdits(contents, lineNumber, coordinateRows, &edits, errorMessage)) {
        return false;
    }

    applyTextEditsPreservingCursor(edits, true, false, true, false);
    return true;
}

void TextEditorSourceRewriteController::replaceTextForCommand(const QString &contents)
{
    if (context_.editor == nullptr) {
        return;
    }

    replaceTextPreservingCursor(contents, true, true, true, false);
}

void TextEditorSourceRewriteController::replaceTextForCommandWithUndo(const QString &contents)
{
    if (context_.editor == nullptr) {
        return;
    }

    replaceTextPreservingCursor(contents, true, true, true, true);
}

void TextEditorSourceRewriteController::replaceTextForSystemNormalization(const QString &contents)
{
    if (context_.editor == nullptr) {
        return;
    }

    replaceTextPreservingCursor(contents, false, true, false, false);
}

void TextEditorSourceRewriteController::replaceEditorText(const QString &contents, bool recordUndoStep)
{
    if (context_.editor == nullptr) {
        return;
    }

    if (context_.editor->toPlainText() == contents) {
        return;
    }

    if (!recordUndoStep) {
        context_.editor->setPlainText(contents);
        return;
    }

    QTextDocument *document = context_.editor->document();
    if (document == nullptr) {
        context_.editor->setPlainText(contents);
        return;
    }

    QTextCursor rewriteCursor(document);
    rewriteCursor.beginEditBlock();
    rewriteCursor.select(QTextCursor::Document);
    rewriteCursor.insertText(contents);
    rewriteCursor.endEditBlock();
}

void TextEditorSourceRewriteController::replaceTextPreservingCursor(const QString &contents,
                                                                    bool emitDocumentTextChanged,
                                                                    bool rebuildBlocksCanvas,
                                                                    bool applyDirtyState,
                                                                    bool recordUndoStep)
{
    if (context_.editor == nullptr) {
        return;
    }

    const QTextCursor previousCursor = context_.editor->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    if (context_.loading != nullptr) {
        *context_.loading = true;
    }
    replaceEditorText(contents, recordUndoStep);

    const int targetLine = qBound(0, previousLine, qMax(0, context_.editor->document()->blockCount() - 1));
    const QTextBlock targetBlock = context_.editor->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        context_.editor->setTextCursor(restoredCursor);
    }

    if (context_.loading != nullptr) {
        *context_.loading = false;
    }
    if (context_.currentLineNumber != nullptr) {
        *context_.currentLineNumber = context_.editor->textCursor().blockNumber() + 1;
    }
    if (applyDirtyState && context_.applyDirtyStateFromCurrentState) {
        context_.applyDirtyStateFromCurrentState();
    }
    if (rebuildBlocksCanvas && context_.rebuildBlocksCanvasFromText) {
        context_.rebuildBlocksCanvasFromText();
    }
    if (emitDocumentTextChanged && context_.documentTextChanged) {
        context_.documentTextChanged();
    }
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
}

void TextEditorSourceRewriteController::applyTextEditsPreservingCursor(QVector<TherionSourceTextEdit> edits,
                                                                       bool emitDocumentTextChanged,
                                                                       bool rebuildBlocksCanvas,
                                                                       bool applyDirtyState,
                                                                       bool recordUndoStep)
{
    if (context_.editor == nullptr || edits.isEmpty()) {
        return;
    }

    QTextDocument *document = context_.editor->document();
    if (document == nullptr) {
        return;
    }

    const int documentLength = context_.editor->toPlainText().size();
    for (const TherionSourceTextEdit &edit : edits) {
        if (edit.startOffset < 0 || edit.length < 0 || edit.startOffset + edit.length > documentLength) {
            return;
        }
    }

    const QTextCursor previousCursor = context_.editor->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    if (context_.loading != nullptr) {
        *context_.loading = true;
    }

    std::sort(edits.begin(), edits.end(), [](const TherionSourceTextEdit &left, const TherionSourceTextEdit &right) {
        return left.startOffset > right.startOffset;
    });

    const bool undoWasEnabled = document->isUndoRedoEnabled();
    if (!recordUndoStep && undoWasEnabled) {
        document->setUndoRedoEnabled(false);
    }

    QTextCursor editCursor(document);
    if (recordUndoStep) {
        editCursor.beginEditBlock();
    }
    for (const TherionSourceTextEdit &edit : edits) {
        editCursor.setPosition(edit.startOffset);
        editCursor.setPosition(edit.startOffset + edit.length, QTextCursor::KeepAnchor);
        editCursor.insertText(edit.replacementText);
    }
    if (recordUndoStep) {
        editCursor.endEditBlock();
    }
    if (!recordUndoStep && undoWasEnabled) {
        document->setUndoRedoEnabled(true);
    }

    const int targetLine = qBound(0, previousLine, qMax(0, context_.editor->document()->blockCount() - 1));
    const QTextBlock targetBlock = context_.editor->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        context_.editor->setTextCursor(restoredCursor);
    }

    if (context_.loading != nullptr) {
        *context_.loading = false;
    }
    if (context_.currentLineNumber != nullptr) {
        *context_.currentLineNumber = context_.editor->textCursor().blockNumber() + 1;
    }
    if (applyDirtyState && context_.applyDirtyStateFromCurrentState) {
        context_.applyDirtyStateFromCurrentState();
    }
    if (rebuildBlocksCanvas && context_.rebuildBlocksCanvasFromText) {
        context_.rebuildBlocksCanvasFromText();
    }
    if (emitDocumentTextChanged && context_.documentTextChanged) {
        context_.documentTextChanged();
    }
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
}

void TextEditorSourceRewriteController::replaceTextSelectingLine(const QString &contents,
                                                                 int lineNumber,
                                                                 bool recordUndoStep)
{
    if (context_.editor == nullptr) {
        return;
    }

    if (context_.loading != nullptr) {
        *context_.loading = true;
    }
    replaceEditorText(contents, recordUndoStep);

    if (lineNumber > 0) {
        const QTextBlock targetBlock = context_.editor->document()->findBlockByLineNumber(lineNumber - 1);
        if (targetBlock.isValid()) {
            QTextCursor cursor(targetBlock);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            context_.editor->setTextCursor(cursor);
            context_.editor->centerCursor();
        }
    }

    if (context_.loading != nullptr) {
        *context_.loading = false;
    }
    if (context_.currentLineNumber != nullptr) {
        *context_.currentLineNumber = context_.editor->textCursor().blockNumber() + 1;
    }
    if (context_.applyDirtyStateFromCurrentState) {
        context_.applyDirtyStateFromCurrentState();
    }
    if (context_.documentTextChanged) {
        context_.documentTextChanged();
    }
    if (context_.updateContextHelp) {
        context_.updateContextHelp();
    }
}
}
