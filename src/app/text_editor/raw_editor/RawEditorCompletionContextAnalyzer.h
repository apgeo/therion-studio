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

struct RawEditorCompletionContext
{
    QPlainTextEdit *editor = nullptr;
    const TextEditorCommandMetadata *metadata = nullptr;
    TherionSourceSnapshotCache *sourceSnapshotCache = nullptr;
    std::function<QString(const QString &)> normalizedDirectiveToken;
    std::function<QString(const QString &)> openingDirectiveForClosingToken;
    std::function<bool(const QString &, const TherionParsedLine &)> isContainerDirectiveInstance;
};

class RawEditorCompletionContextAnalyzer final
{
public:
    explicit RawEditorCompletionContextAnalyzer(RawEditorCompletionContext context);

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

    RawEditorCompletionContext context_;
};
}
