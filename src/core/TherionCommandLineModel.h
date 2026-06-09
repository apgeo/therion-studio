#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace TherionStudio
{
struct CommandOptionEntry
{
    QString key;
    QString value;
};

struct ParsedCommandOptions
{
    QString leadingValue;
    QStringList extraPositionalTokens;
    QVector<CommandOptionEntry> optionEntries;
    int optionsStartIndex = 1;
};

ParsedCommandOptions parseCommandOptions(const QString &commandName,
                                         const QStringList &tokens,
                                         const QHash<QString, int> &commandOptionFixedArityByKey,
                                         bool leadingValueAllowed);

bool commandTokenStartsNewOption(const QString &token);
bool commandTokenEmbedsOptionValue(const QString &token);
QString commandEmbeddedOptionName(const QString &token);
QString commandEmbeddedOptionValue(const QString &token);
int nextCommandOptionIndex(const QStringList &tokens, int optionIndex);
QString normalizedCommandOptionName(const QString &optionName);
bool commandOptionNameMatches(const QString &token, const QString &optionName);
QStringList commandOptionValueTokens(const QStringList &tokens, const QString &optionName);
QString commandOptionValue(const QStringList &tokens, const QString &optionName);
QHash<QString, QString> commandOptionValuesByName(const QStringList &tokens);
std::optional<bool> commandOptionToggleValue(const QStringList &tokens, const QString &optionName);
QString serializeCommandArgumentValues(const QStringList &values);
QStringList serializeCommandOptionTokens(const QString &optionToken, const QStringList &values);
}
