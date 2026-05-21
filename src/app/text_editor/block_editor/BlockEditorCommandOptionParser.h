#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
struct BlockEditorParsedOptionEntry
{
    QString key;
    QString value;
};

struct BlockEditorParsedCommandOptions
{
    QString leadingValue;
    QStringList extraPositionalTokens;
    QVector<BlockEditorParsedOptionEntry> optionEntries;
    int optionsStartIndex = 1;
};

BlockEditorParsedCommandOptions parseBlockEditorCommandOptions(const QString &commandName,
                                                               const QStringList &tokens,
                                                               const QHash<QString, int> &commandOptionFixedArityByKey,
                                                               bool leadingValueAllowed);

bool blockEditorTokenStartsNewOption(const QString &token);
int nextBlockEditorOptionIndex(const QStringList &tokens, int optionIndex);
}
