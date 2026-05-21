#pragma once

#include <QEvent>
#include <QString>
#include <QStringList>

class QJsonObject;

namespace TherionStudio
{
class TextEditorTab;
struct TherionHelpEntry;

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
    void applyCommandArgumentMetadata(const QString &commandName,
                                      const QJsonObject &commandObject,
                                      TherionHelpEntry *entry,
                                      int *requiredPositionalCount,
                                      bool *primaryValueIsPerson,
                                      QStringList *commandArgumentSignatures) const;
    void applyCommandContextMetadata(const QString &commandName,
                                     const QJsonObject &commandObject,
                                     QStringList *normalizedCommandContexts) const;
    void applyCommandOptionCatalogMetadata(const QString &commandName,
                                           const QJsonObject &commandObject,
                                           TherionHelpEntry *entry) const;
    void applyCommandRegistrationMetadata(const QString &commandName,
                                          const TherionHelpEntry &entry,
                                          int requiredPositionalCount,
                                          bool primaryValueIsPerson,
                                          const QStringList &commandArgumentSignatures) const;
    void applyCommandAliasMetadata(const QString &commandName,
                                   const QJsonObject &commandObject,
                                   const TherionHelpEntry &entry,
                                   const QStringList &normalizedCommandContexts) const;
    void applyCatalogCommandsMetadata(const QJsonObject &catalogObject) const;
    void mergeHelpEntry(const QString &token, const TherionHelpEntry &entry) const;
    void registerCompletionToken(const QString &token) const;
    void rebuildCompletionModel() const;
    QStringList projectInputFileCompletionCandidates() const;
    QStringList buildCompletionSuggestionsForCursor(const QString &prefix) const;
    void triggerCompletionPopup();
    void insertCompletionToken(const QString &completion);

private:
    TextEditorTab *owner_ = nullptr;
};
}
