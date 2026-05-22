#pragma once

#include <functional>

#include <QEvent>
#include <QString>
#include <QStringList>

class QJsonObject;
class QObject;
class QCompleter;
class QPlainTextEdit;
class QStringListModel;

namespace TherionStudio
{
struct TextEditorCommandMetadata;
class RawEditorCompletionContextAnalyzer;
struct TherionParsedLine;

struct RawEditorCompletionControllerContext
{
    QObject *eventContext = nullptr;
    QPlainTextEdit *editor = nullptr;
    QCompleter *completionCompleter = nullptr;
    QStringListModel *completionModel = nullptr;
    TextEditorCommandMetadata *metadata = nullptr;
    std::function<QString()> projectRootPath;
    std::function<QString()> filePath;
    std::function<QString(const QString &)> normalizedDirectiveToken;
    std::function<QString(const QString &)> openingDirectiveForClosingToken;
    std::function<QString(const QString &)> closingDirectiveForOpeningToken;
    std::function<bool(const QString &, const TherionParsedLine &)> isContainerDirectiveInstance;
};

class RawEditorCompletionController final
{
public:
    enum class EventHandling
    {
        NotHandled,
        PassToBase,
        Consumed,
    };

    explicit RawEditorCompletionController(RawEditorCompletionControllerContext context);

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
    RawEditorCompletionContextAnalyzer contextAnalyzer() const;

    RawEditorCompletionControllerContext context_;
};
}
