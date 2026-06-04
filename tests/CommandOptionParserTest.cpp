#include "../src/core/TherionCommandLineModel.h"
#include "../src/core/TherionCommandSyntax.h"

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

using TherionStudio::ParsedCommandOptions;
using TherionStudio::commandOptionValueKey;
using TherionStudio::commandTokenStartsNewOption;
using TherionStudio::parseCommandOptions;
using TherionStudio::serializeCommandArgumentValues;
using TherionStudio::serializeCommandOptionTokens;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "CommandOptionParserTest failed: %s\n", message);
        std::exit(1);
    }
}

void parsesMapObjectAttributesAndOptions()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-orientation")), 1);
    arity.insert(commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-text")), 1);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("10.5"),
                                                             QStringLiteral("20.0"),
                                                             QStringLiteral("label"),
                                                             QStringLiteral("-text"),
                                                             QStringLiteral("Entrance"),
                                                             QStringLiteral("-orientation"),
                                                             QStringLiteral("45")},
                                                            arity,
                                                            false);

    require(parsed.leadingValue.isEmpty(), "map object parser must not consume the first point argument as an id");
    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("10.5"), QStringLiteral("20.0"), QStringLiteral("label")}),
            "point positional attributes should be preserved before options");
    require(parsed.optionEntries.size() == 2, "point options should be parsed as option rows");
    require(parsed.optionEntries.at(0).key == QStringLiteral("-text")
                && parsed.optionEntries.at(0).value == QStringLiteral("Entrance"),
            "single-value option should preserve its value");
    require(parsed.optionEntries.at(1).key == QStringLiteral("-orientation")
                && parsed.optionEntries.at(1).value == QStringLiteral("45"),
            "numeric option value should be preserved");
}

void keepsNegativeNumbersAsValues()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("point"), QStringLiteral("-orientation")), 1);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("1"),
                                                             QStringLiteral("2"),
                                                             QStringLiteral("label"),
                                                             QStringLiteral("-orientation"),
                                                             QStringLiteral("-45")},
                                                            arity,
                                                            false);

    require(!commandTokenStartsNewOption(QStringLiteral("-45")), "negative numeric values must not start a new option");
    require(parsed.optionEntries.size() == 1, "negative numeric option value should stay with its option");
    require(parsed.optionEntries.at(0).value == QStringLiteral("-45"), "negative numeric value should be preserved");
}

void keepsBracketedValuesTogether()
{
    QHash<QString, int> arity;

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("revise"),
                                                            {QStringLiteral("revise"),
                                                             QStringLiteral("dd.s@dur.dur_dom"),
                                                             QStringLiteral("-stations"),
                                                             QStringLiteral("[4@monum.dur_dom"),
                                                             QStringLiteral("5@monum.dur_dom"),
                                                             QStringLiteral("6@monum.dur_dom]")},
                                                            arity,
                                                            false);

    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("dd.s@dur.dur_dom")}),
            "revise id should stay as positional argument");
    require(parsed.optionEntries.size() == 1, "bracketed option value should stay in one option row");
    require(parsed.optionEntries.at(0).key == QStringLiteral("-stations"), "revise stations option should be detected");
    require(parsed.optionEntries.at(0).value == QStringLiteral("[4@monum.dur_dom 5@monum.dur_dom 6@monum.dur_dom]"),
            "bracketed stations value should preserve inner spaces");
}

void serializesFixedArityOptionValues()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-clip")), 2);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("line"),
                                                            {QStringLiteral("line"),
                                                             QStringLiteral("wall"),
                                                             QStringLiteral("-clip"),
                                                             QStringLiteral("left wall"),
                                                             QStringLiteral("right")},
                                                            arity,
                                                            false);

    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("wall")}),
            "line type should stay as a positional argument");
    require(parsed.optionEntries.size() == 1, "fixed-arity option should be parsed as one option row");
    require(parsed.optionEntries.at(0).key == QStringLiteral("-clip"), "fixed-arity option key should be preserved");
    require(parsed.optionEntries.at(0).value == QStringLiteral("\"left wall\" right"),
            "fixed-arity values should be serialized with Therion quoting rules");
}

void keepsLeadingValueSeparateWhenAllowed()
{
    QHash<QString, int> arity;
    arity.insert(commandOptionValueKey(QStringLiteral("scrap"), QStringLiteral("-projection")), 1);

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("scrap"),
                                                            {QStringLiteral("scrap"),
                                                             QStringLiteral("s1"),
                                                             QStringLiteral("-projection"),
                                                             QStringLiteral("plan")},
                                                            arity,
                                                            true);

    require(parsed.leadingValue == QStringLiteral("s1"), "leading id value should be separated when allowed");
    require(parsed.optionsStartIndex == 2, "options should start after the leading value");
    require(parsed.extraPositionalTokens.isEmpty(), "leading id should not also appear as a positional token");
    require(parsed.optionEntries.size() == 1
                && parsed.optionEntries.at(0).key == QStringLiteral("-projection")
                && parsed.optionEntries.at(0).value == QStringLiteral("plan"),
            "options after a leading id should still parse normally");
}

void doesNotTreatDashPrefixedTokenAsLeadingValue()
{
    QHash<QString, int> arity;

    const ParsedCommandOptions parsed = parseCommandOptions(QStringLiteral("point"),
                                                            {QStringLiteral("point"),
                                                             QStringLiteral("-45"),
                                                             QStringLiteral("12")},
                                                            arity,
                                                            true);

    require(parsed.leadingValue.isEmpty(), "dash-prefixed first argument should not be consumed as a leading id");
    require(parsed.optionsStartIndex == 1, "options should still start after the command token");
    require(parsed.extraPositionalTokens == QStringList({QStringLiteral("-45"), QStringLiteral("12")}),
            "dash-prefixed numeric positional tokens should stay in positional arguments");
}

void serializesCommandArguments()
{
    require(serializeCommandArgumentValues({QStringLiteral("plain"),
                                            QStringLiteral("left wall"),
                                            QStringLiteral("[a b]")})
                == QStringLiteral("plain \"left wall\" [a b]"),
            "command argument serialization should quote values and preserve bracketed groups");
}

void serializesCommandOptionTokens()
{
    require(serializeCommandOptionTokens(QStringLiteral("-clip"),
                                         {QStringLiteral("left wall"), QStringLiteral("right")})
                == QStringList({QStringLiteral("-clip"), QStringLiteral("\"left wall\" right")}),
            "option token serialization should keep the option key and join serialized values");
    require(serializeCommandOptionTokens(QStringLiteral(""), {QStringLiteral("ignored")}).isEmpty(),
            "empty option keys should not emit serialized option tokens");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    parsesMapObjectAttributesAndOptions();
    keepsNegativeNumbersAsValues();
    keepsBracketedValuesTogether();
    serializesFixedArityOptionValues();
    keepsLeadingValueSeparateWhenAllowed();
    doesNotTreatDashPrefixedTokenAsLeadingValue();
    serializesCommandArguments();
    serializesCommandOptionTokens();
    return 0;
}
