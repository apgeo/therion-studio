#include "TherionCommandLineModel.h"

#include "TherionCommandSyntax.h"
#include "TherionTokenRules.h"

namespace TherionStudio
{
bool commandTokenStartsNewOption(const QString &token)
{
    return TherionTokenRules::tokenStartsOption(token);
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

QString serializeCommandArgumentValues(const QStringList &values)
{
    QStringList serializedValues;
    serializedValues.reserve(values.size());
    for (const QString &value : values) {
        serializedValues.append(serializeTherionArgumentToken(value.trimmed()));
    }
    return serializedValues.join(QLatin1Char(' '));
}

QStringList serializeCommandOptionTokens(const QString &optionToken, const QStringList &values)
{
    QStringList tokens;
    const QString key = optionToken.trimmed();
    if (key.isEmpty()) {
        return tokens;
    }

    tokens.append(key);
    if (!values.isEmpty()) {
        tokens.append(serializeCommandArgumentValues(values));
    }
    return tokens;
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
                optionDisplayValue = serializeCommandArgumentValues(rawOptionValues);
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
