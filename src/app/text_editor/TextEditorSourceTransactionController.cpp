#include "TextEditorSourceTransactionController.h"

#include "TextEditorTab.h"

#include <QCoreApplication>
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

QString staleSourceTransactionMessage()
{
    return QCoreApplication::translate("TherionStudio::TextEditorSourceTransactionController",
                                       "Source transaction skipped: document changed.");
}

bool sourceRevisionMatches(const TextEditorSourceTransactionContext &context,
                          const TextEditorSourceTransactionRequest &request)
{
    if (context.textEditor == nullptr || request.expectedSourceRevision <= 0) {
        return true;
    }
    return context.textEditor->documentRevision() == request.expectedSourceRevision;
}

bool currentTextMatches(const TextEditorSourceTransactionContext &context, const QString &expectedText)
{
    return context.textEditor != nullptr && context.textEditor->text() == expectedText;
}

void reportStaleRequest(const TextEditorSourceTransactionContext &context,
                        const TextEditorSourceTransactionRequest &request)
{
    if (context.statusCallback == nullptr) {
        return;
    }

    context.statusCallback(request.staleStatusMessage.isEmpty()
                               ? staleSourceTransactionMessage()
                               : request.staleStatusMessage);
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
                                    std::function<void()> initialRedoHook,
                                    std::function<void()> undoHook,
                                    std::function<void()> redoHook,
                                    std::function<void(const QString &)> statusCallback)
        : textEditor_(textEditor)
        , beforeText_(std::move(beforeText))
        , afterText_(std::move(afterText))
        , undoStatusMessage_(std::move(undoStatusMessage))
        , redoStatusMessage_(std::move(redoStatusMessage))
        , initialRedoHook_(std::move(initialRedoHook))
        , undoHook_(std::move(undoHook))
        , redoHook_(std::move(redoHook))
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

        applyTextEditorSourceSnapshot(textEditor_, beforeText_);
        if (undoHook_) {
            undoHook_();
        }
        if (statusCallback_ != nullptr && !undoStatusMessage_.isEmpty()) {
            statusCallback_(undoStatusMessage_);
        }
    }

    void redo() override
    {
        if (firstRedo_) {
            firstRedo_ = false;
            if (initialRedoHook_) {
                initialRedoHook_();
            }
            return;
        }
        if (textEditor_ == nullptr) {
            setObsolete(true);
            return;
        }

        applyTextEditorSourceSnapshot(textEditor_, afterText_);
        if (redoHook_) {
            redoHook_();
        }
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
    std::function<void()> initialRedoHook_;
    std::function<void()> undoHook_;
    std::function<void()> redoHook_;
    std::function<void(const QString &)> statusCallback_;
    bool firstRedo_ = true;
};

void pushSnapshotCommand(const TextEditorSourceTransactionContext &context,
                         const TextEditorSourceTransactionRequest &request,
                         const QString &afterText)
{
    if (context.textEditor == nullptr || context.undoStack == nullptr || request.beforeText == afterText) {
        return;
    }

    const auto pushCommand = [&context, &request, &afterText]() {
        context.undoStack->push(new TextEditorSourceSnapshotCommand(context.textEditor,
                                                                    request.label,
                                                                    request.beforeText,
                                                                    afterText,
                                                                    request.undoStatusMessage,
                                                                    request.redoStatusMessage,
                                                                    request.initialRedoHook,
                                                                    request.undoHook,
                                                                    request.redoHook,
                                                                    context.statusCallback));
    };

    if (context.commandApplyInProgress != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context.commandApplyInProgress), true);
        pushCommand();
    } else {
        pushCommand();
    }
}

void applyRequestPolicies(const TextEditorSourceTransactionContext &context,
                          const TextEditorSourceTransactionRequest &request)
{
    switch (request.projectionInvalidationPolicy) {
    case TextEditorSourceProjectionInvalidationPolicy::FlushPendingRefresh:
        if (context.flushPendingRefresh != nullptr) {
            context.flushPendingRefresh();
        }
        break;
    case TextEditorSourceProjectionInvalidationPolicy::CustomHook:
        if (request.projectionInvalidationHook) {
            request.projectionInvalidationHook();
        }
        break;
    case TextEditorSourceProjectionInvalidationPolicy::None:
        break;
    }

    switch (request.selectionRestorePolicy) {
    case TextEditorSourceSelectionRestorePolicy::PreserveCurrentSelection:
        break;
    case TextEditorSourceSelectionRestorePolicy::CustomHook:
        if (request.selectionRestoreHook) {
            request.selectionRestoreHook();
        }
        break;
    }
}
}

TextEditorSourceTransactionController::TextEditorSourceTransactionController(TextEditorSourceTransactionContext context)
    : context_(std::move(context))
{
}

void applyTextEditorSourceSnapshot(TextEditorTab *textEditor, const QString &contents)
{
    if (textEditor == nullptr) {
        return;
    }

    textEditor->applySourceSnapshotForTransaction(contents);
}

TextEditorSourceTransactionResult TextEditorSourceTransactionController::recordSnapshot(
    const TextEditorSourceTransactionRequest &request)
{
    const std::optional<QString> afterText = resolvedAfterText(request);
    if (!afterText.has_value()) {
        return TextEditorSourceTransactionResult::InvalidEdit;
    }
    if (request.beforeText == afterText.value()) {
        return TextEditorSourceTransactionResult::NoChange;
    }
    if (context_.textEditor == nullptr) {
        return TextEditorSourceTransactionResult::Unavailable;
    }

    if (!sourceRevisionMatches(context_, request) || !currentTextMatches(context_, afterText.value())) {
        reportStaleRequest(context_, request);
        return TextEditorSourceTransactionResult::Stale;
    }

    pushSnapshotCommand(context_, request, afterText.value());
    applyRequestPolicies(context_, request);
    return TextEditorSourceTransactionResult::Applied;
}

TextEditorSourceTransactionResult TextEditorSourceTransactionController::applyChangeWithSnapshot(
    const TextEditorSourceTransactionRequest &request)
{
    const std::optional<QString> afterText = resolvedAfterText(request);
    if (!afterText.has_value()) {
        return TextEditorSourceTransactionResult::InvalidEdit;
    }
    if (request.beforeText == afterText.value()) {
        return TextEditorSourceTransactionResult::NoChange;
    }
    if (context_.textEditor == nullptr) {
        return TextEditorSourceTransactionResult::Unavailable;
    }

    if (!sourceRevisionMatches(context_, request) || !currentTextMatches(context_, request.beforeText)) {
        reportStaleRequest(context_, request);
        return TextEditorSourceTransactionResult::Stale;
    }

    if (context_.commandApplyInProgress != nullptr) {
        const QScopedValueRollback<bool> commandGuard((*context_.commandApplyInProgress), true);
        applyTextEditorSourceSnapshot(context_.textEditor, afterText.value());
        if (context_.markSourceChangeOriginatedFromTransaction) {
            context_.markSourceChangeOriginatedFromTransaction();
        }
        pushSnapshotCommand(context_, request, afterText.value());
        applyRequestPolicies(context_, request);
        return TextEditorSourceTransactionResult::Applied;
    }

    applyTextEditorSourceSnapshot(context_.textEditor, afterText.value());
    if (context_.markSourceChangeOriginatedFromTransaction) {
        context_.markSourceChangeOriginatedFromTransaction();
    }
    pushSnapshotCommand(context_, request, afterText.value());
    applyRequestPolicies(context_, request);
    return TextEditorSourceTransactionResult::Applied;
}
}
