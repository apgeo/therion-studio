#include "RawEditorCompletionController.h"

#include "RawEditorCompletionContextAnalyzer.h"
#include "RawEditorCompletionInsertionController.h"
#include "RawEditorCompletionPopupController.h"
#include "RawEditorCompletionSuggestionBuilder.h"
#include "../TextEditorCommandMetadata.h"
#include <utility>

namespace
{
bool isRequiredArgumentSignature(const QString &signature)
{
    const QString trimmed = signature.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    if (trimmed.startsWith(QLatin1Char('['))) {
        return false;
    }
    return trimmed.contains(QLatin1Char('<')) && trimmed.contains(QLatin1Char('>'));
}
}

namespace TherionStudio
{
RawEditorCompletionContextAnalyzer RawEditorCompletionController::contextAnalyzer() const
{
    if (context_.metadata == nullptr) {
        return RawEditorCompletionContextAnalyzer({});
    }

    RawEditorCompletionContext context;
    context.editor = context_.editor;
    context.metadata = &commandMetadata();
    context.normalizedDirectiveToken = context_.normalizedDirectiveToken;
    context.openingDirectiveForClosingToken = context_.openingDirectiveForClosingToken;
    context.isContainerDirectiveInstance = context_.isContainerDirectiveInstance;
    return RawEditorCompletionContextAnalyzer(std::move(context));
}

QStringList RawEditorCompletionController::commandArgumentSignaturesFor(const QString &commandToken) const
{
    if (context_.metadata == nullptr || !context_.normalizedDirectiveToken) {
        return {};
    }
    return mutableCommandMetadata().commandArgumentSignaturesByToken.value(context_.normalizedDirectiveToken(commandToken.trimmed()));
}

bool RawEditorCompletionController::commandHasRequiredIdArgument(const QString &commandToken) const
{
    const QStringList signatures = commandArgumentSignaturesFor(commandToken);
    for (const QString &signature : signatures) {
        if (!isRequiredArgumentSignature(signature)) {
            continue;
        }
        const QString normalizedSignature = signature.trimmed().toLower();
        return normalizedSignature.contains(QStringLiteral("<id>"));
    }
    return false;
}

bool RawEditorCompletionController::commandSupportsInlineIdField(const QString &commandToken) const
{
    if (context_.metadata == nullptr || !context_.normalizedDirectiveToken) {
        return false;
    }

    const QString normalizedCommand = context_.normalizedDirectiveToken(commandToken.trimmed());
    if (commandHasRequiredIdArgument(normalizedCommand)) {
        return true;
    }
    return mutableCommandMetadata().commandOptionTokens.value(normalizedCommand).contains(QStringLiteral("-id"), Qt::CaseInsensitive);
}

void RawEditorCompletionController::insertCompletionToken(const QString &completion)
{
    if (context_.editor == nullptr) {
        return;
    }

    RawEditorCompletionInsertionContext context;
    context.editor = context_.editor;
    context.normalizedDirectiveToken = context_.normalizedDirectiveToken;
    context.closingDirectiveForOpeningToken = context_.closingDirectiveForOpeningToken;
    RawEditorCompletionInsertionController(std::move(context)).insertCompletionToken(completion);
}

QStringList RawEditorCompletionController::projectInputFileCompletionCandidates() const
{
    if (context_.metadata == nullptr) {
        return {};
    }

    RawEditorCompletionSuggestionContext context;
    context.metadata = &commandMetadata();
    context.projectRootPath = context_.projectRootPath ? context_.projectRootPath() : QString();
    context.filePath = context_.filePath ? context_.filePath() : QString();
    return RawEditorCompletionSuggestionBuilder(std::move(context)).projectInputFileCompletionCandidates();
}

QStringList RawEditorCompletionController::buildCompletionSuggestionsForCursor(const QString &prefix) const
{
    if (context_.metadata == nullptr) {
        return {};
    }

    RawEditorCompletionSuggestionContext context;
    context.editor = context_.editor;
    context.metadata = &commandMetadata();
    context.projectRootPath = context_.projectRootPath ? context_.projectRootPath() : QString();
    context.filePath = context_.filePath ? context_.filePath() : QString();
    context.normalizedDirectiveToken = context_.normalizedDirectiveToken;
    context.openingDirectiveForClosingToken = context_.openingDirectiveForClosingToken;
    context.isContainerDirectiveInstance = context_.isContainerDirectiveInstance;
    return RawEditorCompletionSuggestionBuilder(std::move(context)).buildCompletionSuggestionsForCursor(prefix);
}

void RawEditorCompletionController::triggerCompletionPopup()
{
    if (context_.metadata == nullptr) {
        return;
    }

    RawEditorCompletionPopupContext context;
    context.editor = context_.editor;
    context.completionCompleter = context_.completionCompleter;
    context.completionModel = context_.completionModel;
    context.metadata = &commandMetadata();
    context.projectRootPath = context_.projectRootPath ? context_.projectRootPath() : QString();
    context.filePath = context_.filePath ? context_.filePath() : QString();
    context.normalizedDirectiveToken = context_.normalizedDirectiveToken;
    context.openingDirectiveForClosingToken = context_.openingDirectiveForClosingToken;
    context.isContainerDirectiveInstance = context_.isContainerDirectiveInstance;
    RawEditorCompletionPopupController(std::move(context)).triggerCompletionPopup();
}
}
