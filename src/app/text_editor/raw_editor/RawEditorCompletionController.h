#pragma once

#include <QEvent>
#include <QString>
#include <QStringList>

class QJsonObject;

namespace TherionStudio
{
class TextEditorTab;
struct TextEditorCommandMetadata;

class RawEditorCompletionController final
{
public:
    enum class EventHandling
    {
        NotHandled,
        PassToBase,
        Consumed,
    };

    explicit RawEditorCompletionController(TextEditorTab *owner);

    EventHandling handleEventFilter(QObject *watched, QEvent *event);
    QString currentCompletionPrefix() const;
    QStringList activeCompletionScopeStack() const;
    QString currentCompletionCommand() const;
    QString currentCompletionScopeLabel() const;
    QString normalizeCompletionContext(const QString &contextToken) const;
    bool isCompatibleChildKindForBlocks(const QString &parentKind, const QString &childKind) const;
    bool isCommandDirectiveInScope(const QString &directive, const QString &scopeToken) const;
    QStringList commandArgumentSignaturesFor(const QString &commandToken) const;
    bool commandHasRequiredIdArgument(const QString &commandToken) const;
    bool commandSupportsInlineIdField(const QString &commandToken) const;
    QString primaryInsertionScopeForCommand(const QString &commandToken) const;
    QString resolveScopeForCommandAtLine(const QString &commandToken,
                                         const QStringList &lines,
                                         int lineNumber) const;
    void applyCatalogCommandsMetadata(const QJsonObject &catalogObject) const;
    void rebuildCompletionModel() const;
    QStringList projectInputFileCompletionCandidates() const;
    QStringList buildCompletionSuggestionsForCursor(const QString &prefix) const;
    void triggerCompletionPopup();
    void insertCompletionToken(const QString &completion);

private:
    const TextEditorCommandMetadata &commandMetadata() const;
    TextEditorCommandMetadata &mutableCommandMetadata() const;

    TextEditorTab *owner_ = nullptr;
};
}
