#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TextEditorTab;
struct TextEditorCommandMetadata;

class RawEditorCompletionContextAnalyzer final
{
public:
    explicit RawEditorCompletionContextAnalyzer(TextEditorTab *owner);

    QString currentCompletionPrefix() const;
    QStringList activeCompletionScopeStack() const;
    QString currentCompletionCommand() const;
    QString currentCompletionScopeLabel() const;
    QString normalizeCompletionContext(const QString &contextToken) const;
    bool isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const;
    bool isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const;
    QString primaryInsertionScopeForCommand(const QString &commandToken) const;
    QString resolveScopeForCommandAtLine(const QString &commandToken,
                                         const QStringList &lines,
                                         int lineNumber) const;

private:
    const TextEditorCommandMetadata &metadata() const;

    TextEditorTab *owner_ = nullptr;
};
}
