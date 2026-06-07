#include "TherionCommandLineModel.h"

#include "TherionCommandSyntax.h"
#include "TherionTokenRules.h"

namespace TherionStudio
{
bool commandTokenStartsNewOption(const QString &token)
{
    return TherionTokenRules::tokenStartsOption(token);
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
