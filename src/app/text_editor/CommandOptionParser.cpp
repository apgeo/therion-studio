#include "CommandOptionParser.h"

#include "../../core/TherionCommandSyntax.h"

namespace
{
bool optionTokenLooksNumeric(QString token)
{
    token = token.trimmed();
    while (token.startsWith(QLatin1Char('['))) {
        token.remove(0, 1);
    }
    while (token.endsWith(QLatin1Char(']'))) {
        token.chop(1);
    }
    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    token.toDouble(&ok);
    return ok;
}
}

namespace TherionStudio
{
bool commandTokenStartsNewOption(const QString &token)
{
    const QString trimmed = token.trimmed();
    return trimmed.startsWith(QLatin1Char('-'))
        && !optionTokenLooksNumeric(trimmed);
}

int nextCommandOptionIndex(const QStringList &tokens, int optionIndex)
{
    bool inBracketedValue = false;
    for (int scan = optionIndex + 1; scan < tokens.size(); ++scan) {
        const QString token = tokens.at(scan).trimmed();
        if (inBracketedValue) {
            if (token.contains(QLatin1Char(']'))) {
                inBracketedValue = false;
            }
            continue;
        }

        if (scan == optionIndex + 1 && token.contains(QLatin1Char('[')) && !token.contains(QLatin1Char(']'))) {
            inBracketedValue = true;
            continue;
        }

        if (commandTokenStartsNewOption(token)) {
            return scan;
        }
    }

    return tokens.size();
}

ParsedCommandOptions parseCommandOptions(
    const QString &commandName,
    const QStringList &tokens,
    const QHash<QString, int> &commandOptionFixedArityByKey,
    bool leadingValueAllowed)
{
    ParsedCommandOptions parsed;
    if (leadingValueAllowed
        && tokens.size() > 1
        && !tokens.at(1).trimmed().startsWith(QLatin1Char('-'))) {
        parsed.leadingValue = tokens.at(1).trimmed();
        parsed.optionsStartIndex = 2;
    }

    for (int index = parsed.optionsStartIndex; index < tokens.size();) {
        const QString token = tokens.at(index).trimmed();
        if (commandTokenStartsNewOption(token)) {
            const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
            const QStringList rawOptionValues = tokens.mid(index + 1, nextOptionIndex - index - 1);
            QString optionDisplayValue = rawOptionValues.join(QLatin1Char(' '));
            const int fixedArity = commandOptionFixedArityByKey.value(
                commandOptionValueKey(commandName, token.toLower().trimmed()), -1);
            if (fixedArity > 1 && !rawOptionValues.isEmpty()) {
                QStringList serializedValues;
                serializedValues.reserve(rawOptionValues.size());
                for (const QString &rawOptionValue : rawOptionValues) {
                    serializedValues.append(serializeTherionArgumentToken(rawOptionValue));
                }
                optionDisplayValue = serializedValues.join(QLatin1Char(' '));
            }
            parsed.optionEntries.append(CommandOptionEntry{token, optionDisplayValue});
            index = nextOptionIndex;
            continue;
        }

        parsed.extraPositionalTokens.append(token);
        ++index;
    }

    return parsed;
}
}
