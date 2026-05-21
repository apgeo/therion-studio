#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TextEditorTab;
struct TextEditorCommandMetadata;

class RawEditorCompletionSuggestionBuilder final
{
public:
    explicit RawEditorCompletionSuggestionBuilder(TextEditorTab *owner);

    QString normalizeInputCompletionPrefix(QString prefix) const;
    QStringList projectInputFileCompletionCandidates() const;
    QStringList buildCompletionSuggestionsForCursor(const QString &prefix) const;

private:
    const TextEditorCommandMetadata &metadata() const;

    TextEditorTab *owner_ = nullptr;
};
}
