#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

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
int nextCommandOptionIndex(const QStringList &tokens, int optionIndex);
}
