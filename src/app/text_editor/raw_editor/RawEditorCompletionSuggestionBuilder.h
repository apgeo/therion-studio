#pragma once

#include <functional>

#include <QString>
#include <QStringList>

class QPlainTextEdit;

namespace TherionStudio
{
struct TextEditorCommandMetadata;
class TherionSourceSnapshotCache;
struct TherionParsedLine;
struct RawEditorCompletionContext;

struct RawEditorCompletionSuggestionContext
{
    QPlainTextEdit *editor = nullptr;
    const TextEditorCommandMetadata *metadata = nullptr;
    TherionSourceSnapshotCache *sourceSnapshotCache = nullptr;
    QString projectRootPath;
    QString filePath;
    std::function<QString(const QString &)> normalizedDirectiveToken;
    std::function<QString(const QString &)> openingDirectiveForClosingToken;
    std::function<bool(const QString &, const TherionParsedLine &)> isContainerDirectiveInstance;
};

class RawEditorCompletionSuggestionBuilder final
{
public:
    explicit RawEditorCompletionSuggestionBuilder(RawEditorCompletionSuggestionContext context);

    QString normalizeInputCompletionPrefix(QString prefix) const;
    QStringList projectInputFileCompletionCandidates() const;
    QStringList buildCompletionSuggestionsForCursor(const QString &prefix) const;

private:
    const TextEditorCommandMetadata &metadata() const;
    RawEditorCompletionContext completionContext() const;

    RawEditorCompletionSuggestionContext context_;
};
}
