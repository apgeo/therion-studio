#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
struct TherionHelpEntry
{
    QString summary;
    QString syntax;
    QStringList arguments;
    QStringList acceptedValues;
    QStringList options;
    QStringList relatedKeywords;
};

struct TextEditorCommandMetadata
{
    QHash<QString, TherionHelpEntry> helpEntries;
    QStringList completionTokens;
    QStringList commandCompletionTokens;
    QHash<QString, QStringList> commandOptionTokens;
    QHash<QString, QStringList> commandValueTokens;
    QHash<QString, QStringList> commandArgumentValueTokens;
    QHash<QString, QStringList> commandOptionValueTokens;
    QHash<QString, QString> commandOptionValueArityTokens;
    QHash<QString, QStringList> commandOptionArgumentLabelsByKey;
    QHash<QString, int> commandOptionFixedArityByKey;
    QHash<QString, QString> commandOptionHelpHtmlByKey;
    QHash<QString, QStringList> commandTypeValueTokens;
    QHash<QString, QHash<QString, QStringList>> commandSubtypeByTypeTokens;
    QHash<QString, int> commandRequiredPositionalCount;
    QHash<QString, QStringList> commandArgumentSignaturesByToken;
    QHash<QString, bool> commandPrimaryValueIsPerson;
    QHash<QString, QStringList> contextCommandTokens;
    QHash<QString, QStringList> blockCommandContextsByKind;

    void clear()
    {
        helpEntries.clear();
        completionTokens.clear();
        commandCompletionTokens.clear();
        commandOptionTokens.clear();
        commandValueTokens.clear();
        commandArgumentValueTokens.clear();
        commandOptionValueTokens.clear();
        commandOptionValueArityTokens.clear();
        commandOptionArgumentLabelsByKey.clear();
        commandOptionFixedArityByKey.clear();
        commandOptionHelpHtmlByKey.clear();
        commandTypeValueTokens.clear();
        commandSubtypeByTypeTokens.clear();
        commandRequiredPositionalCount.clear();
        commandArgumentSignaturesByToken.clear();
        commandPrimaryValueIsPerson.clear();
        contextCommandTokens.clear();
        blockCommandContextsByKind.clear();
    }
};
}
