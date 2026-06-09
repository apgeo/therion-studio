#include "TherionCommandLineModel.h"

#include "TherionCommandSyntax.h"
#include "TherionTokenRules.h"

namespace TherionStudio
{
bool commandTokenStartsNewOption(const QString &token)
{
    return TherionTokenRules::tokenStartsOption(token);
}

QString commandEmbeddedOptionName(const QString &token)
{
    const QString trimmed = token.trimmed();
    for (int index = 0; index < trimmed.size(); ++index) {
        if (trimmed.at(index).isSpace()) {
            return trimmed.left(index);
        }
    }

    return trimmed;
}

QString commandEmbeddedOptionValue(const QString &token)
{
    const QString trimmed = token.trimmed();
    for (int index = 0; index < trimmed.size(); ++index) {
        if (trimmed.at(index).isSpace()) {
            return trimmed.mid(index + 1).trimmed();
        }
    }

    return QString();
}

bool commandTokenEmbedsOptionValue(const QString &token)
{
    return commandTokenStartsNewOption(token)
        && !commandEmbeddedOptionValue(token).isEmpty();
}

QString normalizedCommandOptionName(const QString &optionName)
{
    QString normalized = optionName.trimmed().toLower();
    while (normalized.startsWith(QLatin1Char('-'))) {
        normalized.remove(0, 1);
    }
    return normalized;
}

bool commandOptionNameMatches(const QString &token, const QString &optionName)
{
    const QString normalizedToken = normalizedCommandOptionName(token);
    const QString normalizedOption = normalizedCommandOptionName(optionName);
    return !normalizedToken.isEmpty() && normalizedToken == normalizedOption;
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

QStringList commandOptionValueTokens(const QStringList &tokens, const QString &optionName)
{
    if (optionName.trimmed().isEmpty()) {
        return {};
    }

    for (int index = 0; index < tokens.size(); ++index) {
        if (!commandOptionNameMatches(tokens.at(index), optionName)) {
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
        return tokens.mid(index + 1, nextOptionIndex - index - 1);
    }

    return {};
}

QString commandOptionValue(const QStringList &tokens, const QString &optionName)
{
    const QStringList values = commandOptionValueTokens(tokens, optionName);
    return values.isEmpty() ? QString() : values.constFirst().trimmed();
}

QHash<QString, QString> commandOptionValuesByName(const QStringList &tokens)
{
    QHash<QString, QString> values;
    for (int index = 0; index < tokens.size(); ++index) {
        const QString token = tokens.at(index).trimmed();
        if (!commandTokenStartsNewOption(token)) {
            continue;
        }

        const QString fieldName = normalizedCommandOptionName(token);
        if (fieldName.isEmpty()) {
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
        const QStringList rawOptionValues = tokens.mid(index + 1, nextOptionIndex - index - 1);
        if (rawOptionValues.isEmpty()) {
            continue;
        }

        values.insert(fieldName, rawOptionValues.join(QLatin1Char(' ')).trimmed());
        index = nextOptionIndex - 1;
    }
    return values;
}

namespace
{
CommandOptionEntry commandOptionEntry(const QString &commandName,
                                      const QString &token,
                                      const QStringList &rawOptionValues,
                                      const QHash<QString, int> &commandOptionFixedArityByKey)
{
    QString optionKey = token.trimmed();
    QString optionDisplayValue;
    const QString embeddedValue = commandEmbeddedOptionValue(optionKey);
    if (!embeddedValue.isEmpty()) {
        optionDisplayValue = embeddedValue;
        optionKey = commandEmbeddedOptionName(optionKey);
    } else {
        optionDisplayValue = rawOptionValues.join(QLatin1Char(' '));
        const int fixedArity = commandOptionFixedArityByKey.value(
            commandOptionValueKey(commandName, optionKey.toLower().trimmed()), -1);
        if (fixedArity > 1 && !rawOptionValues.isEmpty()) {
            optionDisplayValue = serializeCommandArgumentValues(rawOptionValues);
        }
    }

    return CommandOptionEntry{optionKey, optionDisplayValue};
}

QString commandOptionEntryDeduplicationKey(const CommandOptionEntry &entry)
{
    return normalizedCommandOptionName(entry.key)
        + QLatin1Char('\n')
        + entry.value.trimmed();
}

std::optional<bool> parseCommandToggleToken(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    if (normalized == QStringLiteral("on")
        || normalized == QStringLiteral("yes")
        || normalized == QStringLiteral("true")
        || normalized == QStringLiteral("1")) {
        return true;
    }
    if (normalized == QStringLiteral("off")
        || normalized == QStringLiteral("no")
        || normalized == QStringLiteral("false")
        || normalized == QStringLiteral("0")) {
        return false;
    }

    return std::nullopt;
}
}

std::optional<bool> commandOptionToggleValue(const QStringList &tokens, const QString &optionName)
{
    std::optional<bool> value;
    if (optionName.trimmed().isEmpty()) {
        return value;
    }

    for (int index = 0; index < tokens.size(); ++index) {
        if (!commandOptionNameMatches(tokens.at(index), optionName)) {
            continue;
        }

        const int nextOptionIndex = nextCommandOptionIndex(tokens, index);
        if (nextOptionIndex <= index + 1) {
            value = true;
        } else {
            value = parseCommandToggleToken(tokens.at(index + 1)).value_or(true);
        }
        index = nextOptionIndex - 1;
    }

    return value;
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
    QStringList seenOptionEntries;
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
            const CommandOptionEntry entry = commandOptionEntry(commandName,
                                                               token,
                                                               rawOptionValues,
                                                               commandOptionFixedArityByKey);
            const QString deduplicationKey = commandOptionEntryDeduplicationKey(entry);
            if (!seenOptionEntries.contains(deduplicationKey)) {
                parsed.optionEntries.append(entry);
                seenOptionEntries.append(deduplicationKey);
            }
            index = nextOptionIndex;
            continue;
        }

        parsed.extraPositionalTokens.append(token);
        ++index;
    }

    return parsed;
}
}
