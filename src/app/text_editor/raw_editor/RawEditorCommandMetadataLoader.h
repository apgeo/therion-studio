#pragma once

#include <functional>

#include <QString>
#include <QStringList>

class QJsonObject;
class QStringListModel;

namespace TherionStudio
{
struct TextEditorCommandMetadata;
struct TherionHelpEntry;

struct RawEditorCommandMetadataContext
{
    TextEditorCommandMetadata *metadata = nullptr;
    QStringListModel *completionModel = nullptr;
    std::function<QString(const QString &)> normalizedDirectiveToken;
};

class RawEditorCommandMetadataLoader final
{
public:
    explicit RawEditorCommandMetadataLoader(RawEditorCommandMetadataContext context);

    void applyCatalogCommandsMetadata(const QJsonObject &catalogObject) const;
    void rebuildCompletionModel() const;

private:
    TextEditorCommandMetadata &metadata() const;
    QString normalizeCompletionContext(const QString &contextToken) const;
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
                                          const QStringList &commandArgumentSignatures,
                                          const QString &sourceFile) const;
    void applyCommandAliasMetadata(const QString &commandName,
                                   const QJsonObject &commandObject,
                                   const TherionHelpEntry &entry,
                                   const QStringList &normalizedCommandContexts) const;
    void mergeHelpEntry(const QString &token, const TherionHelpEntry &entry) const;
    void registerCompletionToken(const QString &token) const;

    RawEditorCommandMetadataContext context_;
};
}
