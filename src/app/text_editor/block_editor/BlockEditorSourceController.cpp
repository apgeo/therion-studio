#include "BlockEditorSourceController.h"

#include "../../../core/TherionDocumentEditor.h"
#include <QCoreApplication>

#include "BlockEditorSourceText.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>

#include <utility>

namespace TherionStudio
{
BlockEditorSourceController::BlockEditorSourceController(BlockEditorSourceContext context)
    : context_(std::move(context))
{
}

QString BlockEditorSourceController::tr(const char *text) const
{
    return QCoreApplication::translate("TherionStudio::BlockEditorSourceController", text);
}

bool BlockEditorSourceController::canEdit() const
{
    return context_.editable && context_.editor != nullptr;
}

bool BlockEditorSourceController::hasEditor() const
{
    return context_.editor != nullptr;
}

QString BlockEditorSourceController::text() const
{
    return hasEditor() ? context_.editor->toPlainText() : QString();
}

QStringList BlockEditorSourceController::normalizedLines() const
{
    return blockEditorNormalizedSourceLines(text());
}

bool BlockEditorSourceController::insertLinesBefore(int lineNumber,
                                                    const QStringList &newLines,
                                                    QString *errorMessage) const
{
    if (!canEdit() || lineNumber <= 0 || newLines.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Invalid insertion request.");
        }
        return false;
    }

    QStringList trimmedLines;
    trimmedLines.reserve(newLines.size());
    for (const QString &line : newLines) {
        const QString candidate = line.trimmed();
        if (candidate.isEmpty()) {
            continue;
        }
        trimmedLines.append(line);
    }
    if (trimmedLines.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("No lines to insert.");
        }
        return false;
    }

    const QString contents = text();
    QStringList lines = blockEditorNormalizedSourceLines(contents);
    const int insertIndex = qBound(0, lineNumber - 1, lines.size());
    for (int offset = 0; offset < trimmedLines.size(); ++offset) {
        lines.insert(insertIndex + offset, trimmedLines.at(offset));
    }

    TherionSourceTextEdit sourceEdit;
    if (!blockEditorSourceReplacementEdit(contents, blockEditorJoinSourceLines(contents, lines), &sourceEdit)) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Unable to prepare insertion edit.");
        }
        return false;
    }
    if (!applyTextEdit(sourceEdit)) {
        if (errorMessage != nullptr) {
            *errorMessage = tr("Unable to insert lines.");
        }
        return false;
    }
    return true;
}

bool BlockEditorSourceController::removeLineRange(int startLine, int endLine) const
{
    if (!canEdit() || startLine <= 0 || endLine < startLine) {
        return false;
    }

    TherionSourceTextEdit sourceEdit;
    if (!blockEditorSourceLineRangeReplacementEdit(text(), startLine, endLine, QStringList(), &sourceEdit)) {
        return false;
    }
    return applyTextEdit(sourceEdit);
}

bool BlockEditorSourceController::replaceLine(int lineNumber, const QString &line) const
{
    if (!canEdit() || lineNumber <= 0) {
        return false;
    }

    TherionSourceTextEdit sourceEdit;
    if (!blockEditorSourceLineRangeReplacementEdit(text(), lineNumber, lineNumber, QStringList{line}, &sourceEdit)) {
        return false;
    }
    return applyTextEdit(sourceEdit);
}

bool BlockEditorSourceController::applyTextEdit(const TherionSourceTextEdit &edit) const
{
    if (!canEdit() || context_.editor->document() == nullptr) {
        return false;
    }

    const int documentLength = context_.editor->toPlainText().size();
    if (edit.startOffset < 0 || edit.length < 0 || edit.startOffset + edit.length > documentLength) {
        return false;
    }

    QTextCursor editCursor(context_.editor->document());
    editCursor.beginEditBlock();
    editCursor.setPosition(edit.startOffset);
    editCursor.setPosition(edit.startOffset + edit.length, QTextCursor::KeepAnchor);
    editCursor.insertText(edit.replacementText);
    editCursor.endEditBlock();
    context_.editor->setTextCursor(editCursor);
    return true;
}

}
