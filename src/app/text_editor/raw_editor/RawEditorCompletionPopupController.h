#pragma once

#include <functional>

#include <QString>

class QCompleter;
class QPlainTextEdit;
class QStringListModel;

namespace TherionStudio
{
struct TextEditorCommandMetadata;
struct TherionParsedLine;
struct RawEditorCompletionContext;
struct RawEditorCompletionSuggestionContext;

struct RawEditorCompletionPopupContext
{
    QPlainTextEdit *editor = nullptr;
    QCompleter *completionCompleter = nullptr;
    QStringListModel *completionModel = nullptr;
    const TextEditorCommandMetadata *metadata = nullptr;
    QString projectRootPath;
    QString filePath;
    std::function<QString(const QString &)> normalizedDirectiveToken;
    std::function<QString(const QString &)> openingDirectiveForClosingToken;
    std::function<bool(const QString &, const TherionParsedLine &)> isContainerDirectiveInstance;
};

class RawEditorCompletionPopupController final
{
public:
    explicit RawEditorCompletionPopupController(RawEditorCompletionPopupContext context);

    void triggerCompletionPopup();

private:
    RawEditorCompletionContext completionContext() const;
    RawEditorCompletionSuggestionContext suggestionContext() const;

    RawEditorCompletionPopupContext context_;
};
}
