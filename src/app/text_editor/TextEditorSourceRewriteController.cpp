#include "TextEditorSourceRewriteController.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>

#include <utility>

#include "../../core/TherionDocumentEditor.h"

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

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewriteStructureEntryName(&contents, lineNumber, category, newName, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, false, false, true, false);
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

    QString contents = context_.editor->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendScrapBlock(&contents, preferredName, &resolvedLineNumber, errorMessage, options)) {
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
                                                            const TherionDraftObjectOptions &objectOptions)
{
    if (context_.editor == nullptr) {
        return false;
    }

    QString contents = context_.editor->toPlainText();
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
                                                                const TherionDraftObjectOptions &objectOptions)
{
    if (context_.editor == nullptr) {
        return false;
    }

    QString contents = context_.editor->toPlainText();
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
                                                                const TherionDraftObjectOptions &objectOptions)
{
    if (context_.editor == nullptr) {
        return false;
    }

    QString contents = context_.editor->toPlainText();
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

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewritePointCoordinates(&contents, lineNumber, point, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false, true, false);
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

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewriteLineAreaVertex(&contents, lineNumber, kind, vertexIndex, point, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false, true, false);
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

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewriteLineOptionToggle(&contents, lineNumber, optionName, enabled, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false, true, false);
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

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewritePointOrientation(&contents,
                                                        lineNumber,
                                                        enabled,
                                                        orientationDegrees,
                                                        errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false, true, false);
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

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewriteLinePointOrientation(&contents,
                                                            lineNumber,
                                                            sourceVertexIndex,
                                                            enabled,
                                                            orientationDegrees,
                                                            errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false, true, false);
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

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewriteLinePointLeftSize(&contents,
                                                         lineNumber,
                                                         sourceVertexIndex,
                                                         enabled,
                                                         sizeValue,
                                                         errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLineCoordinateRows(int lineNumber,
                                                                  const QStringList &coordinateRows,
                                                                  QString *errorMessage)
{
    if (context_.editor == nullptr) {
        return false;
    }

    QString contents = context_.editor->toPlainText();
    if (!TherionDocumentEditor::rewriteLineCoordinateRows(&contents, lineNumber, coordinateRows, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false, true, false);
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
