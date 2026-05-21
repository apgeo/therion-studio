#include "TextEditorSourceRewriteController.h"

#include "TextEditorTab.h"

#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>

#include "../../core/TherionDocumentEditor.h"

namespace TherionStudio
{
TextEditorSourceRewriteController::TextEditorSourceRewriteController(TextEditorTab *owner)
    : owner_(owner)
{
}

bool TextEditorSourceRewriteController::rewriteStructureEntryName(int lineNumber,
                                                                  const QString &category,
                                                                  const QString &newName,
                                                                  QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteStructureEntryName(&contents, lineNumber, category, newName, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, false, false);
    return true;
}

bool TextEditorSourceRewriteController::insertScrapBlock(const QString &preferredName,
                                                         int *insertedLineNumber,
                                                         QString *errorMessage,
                                                         const QString &options)
{
    QString contents = owner_->editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendScrapBlock(&contents, preferredName, &resolvedLineNumber, errorMessage, options)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::insertDraftGeometry(const QString &kind,
                                                            const QVector<QPointF> &vertices,
                                                            int *insertedLineNumber,
                                                            QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftGeometry(&contents, kind, vertices, &resolvedLineNumber, errorMessage)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::insertDraftLineGeometry(const QStringList &coordinateRows,
                                                                int *insertedLineNumber,
                                                                QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftLineGeometry(&contents, coordinateRows, &resolvedLineNumber, errorMessage)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::insertDraftAreaGeometry(const QStringList &coordinateRows,
                                                                int *insertedLineNumber,
                                                                QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    int resolvedLineNumber = 0;
    if (!TherionDocumentEditor::appendDraftAreaGeometry(&contents, coordinateRows, &resolvedLineNumber, errorMessage)) {
        return false;
    }

    replaceTextSelectingLine(contents, resolvedLineNumber);
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = resolvedLineNumber;
    }
    return true;
}

bool TextEditorSourceRewriteController::rewritePointCoordinates(int lineNumber,
                                                                const QPointF &point,
                                                                QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewritePointCoordinates(&contents, lineNumber, point, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLineAreaVertex(int lineNumber,
                                                              const QString &kind,
                                                              int vertexIndex,
                                                              const QPointF &point,
                                                              QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLineAreaVertex(&contents, lineNumber, kind, vertexIndex, point, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLineOptionToggle(int lineNumber,
                                                                const QString &optionName,
                                                                bool enabled,
                                                                QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLineOptionToggle(&contents, lineNumber, optionName, enabled, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewritePointOrientation(int lineNumber,
                                                                bool enabled,
                                                                qreal orientationDegrees,
                                                                QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewritePointOrientation(&contents,
                                                        lineNumber,
                                                        enabled,
                                                        orientationDegrees,
                                                        errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLinePointOrientation(int lineNumber,
                                                                    int sourceVertexIndex,
                                                                    bool enabled,
                                                                    qreal orientationDegrees,
                                                                    QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLinePointOrientation(&contents,
                                                            lineNumber,
                                                            sourceVertexIndex,
                                                            enabled,
                                                            orientationDegrees,
                                                            errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLinePointLeftSize(int lineNumber,
                                                                 int sourceVertexIndex,
                                                                 bool enabled,
                                                                 qreal sizeValue,
                                                                 QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLinePointLeftSize(&contents,
                                                         lineNumber,
                                                         sourceVertexIndex,
                                                         enabled,
                                                         sizeValue,
                                                         errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false);
    return true;
}

bool TextEditorSourceRewriteController::rewriteLineCoordinateRows(int lineNumber,
                                                                  const QStringList &coordinateRows,
                                                                  QString *errorMessage)
{
    QString contents = owner_->editor_->toPlainText();
    if (!TherionDocumentEditor::rewriteLineCoordinateRows(&contents, lineNumber, coordinateRows, errorMessage)) {
        return false;
    }

    replaceTextPreservingCursor(contents, true, false);
    return true;
}

void TextEditorSourceRewriteController::replaceTextForCommand(const QString &contents)
{
    replaceTextPreservingCursor(contents, true, true);
}

void TextEditorSourceRewriteController::replaceTextPreservingCursor(const QString &contents,
                                                                    bool emitDocumentTextChanged,
                                                                    bool rebuildBlocksCanvas)
{
    const QTextCursor previousCursor = owner_->editor_->textCursor();
    const int previousLine = previousCursor.blockNumber();
    const int previousColumn = previousCursor.positionInBlock();

    owner_->loading_ = true;
    owner_->editor_->setPlainText(contents);

    const int targetLine = qBound(0, previousLine, qMax(0, owner_->editor_->document()->blockCount() - 1));
    const QTextBlock targetBlock = owner_->editor_->document()->findBlockByLineNumber(targetLine);
    if (targetBlock.isValid()) {
        QTextCursor restoredCursor(targetBlock);
        restoredCursor.movePosition(QTextCursor::StartOfBlock);
        restoredCursor.movePosition(QTextCursor::Right,
                                    QTextCursor::MoveAnchor,
                                    qMax(0, qMin(previousColumn, targetBlock.length() > 0 ? targetBlock.length() - 1 : 0)));
        owner_->editor_->setTextCursor(restoredCursor);
    }

    owner_->loading_ = false;
    owner_->currentLineNumber_ = owner_->editor_->textCursor().blockNumber() + 1;
    owner_->applyDirtyStateFromCurrentState();
    if (rebuildBlocksCanvas) {
        owner_->rebuildBlocksCanvasFromText();
    }
    if (emitDocumentTextChanged) {
        emit owner_->documentTextChanged();
    }
    owner_->updateContextHelp();
}

void TextEditorSourceRewriteController::replaceTextSelectingLine(const QString &contents, int lineNumber)
{
    owner_->loading_ = true;
    owner_->editor_->setPlainText(contents);

    if (lineNumber > 0) {
        const QTextBlock targetBlock = owner_->editor_->document()->findBlockByLineNumber(lineNumber - 1);
        if (targetBlock.isValid()) {
            QTextCursor cursor(targetBlock);
            cursor.movePosition(QTextCursor::StartOfBlock);
            cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
            owner_->editor_->setTextCursor(cursor);
            owner_->editor_->centerCursor();
        }
    }

    owner_->loading_ = false;
    owner_->currentLineNumber_ = owner_->editor_->textCursor().blockNumber() + 1;
    owner_->applyDirtyStateFromCurrentState();
    emit owner_->documentTextChanged();
    owner_->updateContextHelp();
}
}
