#include "TextEditorSourceTransactionController.h"

#include "TextEditorTab.h"

#include <QPointer>
#include <QScopedValueRollback>
#include <QUndoCommand>
#include <QUndoStack>

#include <optional>
#include <utility>

namespace TherionStudio
{
namespace
{
std::optional<QString> resolvedAfterText(const TextEditorSourceTransactionRequest &request)
{
    if (request.sourceEdits.isEmpty()) {
        return request.afterText;
    }

    QString afterText = request.beforeText;
    if (!TherionDocumentEditor::applySourceTextEdits(&afterText, request.sourceEdits)) {
        return std::nullopt;
    }
    return afterText;
}

class TextEditorSourceSnapshotCommand final : public QUndoCommand
{
public:
    TextEditorSourceSnapshotCommand(TextEditorTab *textEditor,
                                    QString label,
                                    QString beforeText,
                                    QString afterText,
                                    QString undoStatusMessage,
                                    QString redoStatusMessage,
                                    std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , beforeText_(std::move(beforeText))
        , afterText_(std::move(afterText))
        , undoStatusMessage_(std::move(undoStatusMessage))
        , redoStatusMessage_(std::move(redoStatusMessage))
        , statusCallback_(std::move(statusCallback))
    {
        setText(std::move(label));
    }

    void undo() override
    {
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        textEditor_->replaceTextForCommand(beforeText_);
        if (statusCallback_ != nullptr && !undoStatusMessage_.isEmpty()) {
            statusCallback_(undoStatusMessage_);
        }
    }

    void redo() override
    {
        if (firstRedo_) {
            firstRedo_ = false;
            return;
        }
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        textEditor_->replaceTextForCommand(afterText_);
        if (statusCallback_ != nullptr && !redoStatusMessage_.isEmpty()) {
            statusCallback_(redoStatusMessage_);
        }
    }

private:
    QPointer<TextEditorTab> textEditor_;
    QString beforeText_;
    QString afterText_;
    QString undoStatusMessage_;
    QString redoStatusMessage_;
    std::function<void(const QString &)> statusCallback_;
    bool firstRedo_ = true;
};
}

TextEditorSourceTransactionController::TextEditorSourceTransactionController(TextEditorSourceTransactionContext context)
    : context_(std::move(context))
{
}

void TextEditorSourceTransactionController::recordSnapshot(const TextEditorSourceTransactionRequest &request)
{
    const std::optional<QString> afterText = resolvedAfterText(request);
    if (!afterText.has_value()
        || context_.textEditor == nullptr
        || context_.undoStack == nullptr
        || request.beforeText == afterText.value()) {
        return;
    }

    const auto pushCommand = [this, &request, afterText = afterText.value()]() {
        context_.undoStack->push(new TextEditorSourceSnapshotCommand(context_.textEditor,
                                                                     request.label,
                                                                     request.beforeText,
                                                                     afterText,
                                                                     request.undoStatusMessage,
                                                                     request.redoStatusMessage,
                                                                     context_.statusCallback));
    };

    if (context_.commandApplyInProgress != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        pushCommand();
    } else {
        pushCommand();
    }

    if (context_.flushPendingRefresh != nullptr) {
        context_.flushPendingRefresh();
    }
}

void TextEditorSourceTransactionController::applyChangeWithSnapshot(const TextEditorSourceTransactionRequest &request)
{
    const std::optional<QString> afterText = resolvedAfterText(request);
    if (!afterText.has_value()
        || context_.textEditor == nullptr
        || request.beforeText == afterText.value()) {
        return;
    }

    if (context_.commandApplyInProgress != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        context_.textEditor->replaceTextForCommand(afterText.value());
        recordSnapshot(request);
        return;
    }

    context_.textEditor->replaceTextForCommand(afterText.value());
    recordSnapshot(request);
}
}
