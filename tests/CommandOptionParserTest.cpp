#include "../src/app/text_editor/CommandOptionParser.h"

#include <QCoreApplication>
#include <QHash>
#include <QString>
#include <QStringList>

#include <cstdio>
#include <cstdlib>

using TherionStudio::ParsedCommandOptions;
using TherionStudio::commandTokenStartsNewOption;
using TherionStudio::parseCommandOptions;

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
    arity.insert(QStringLiteral("point:-orientation"), 1);
    arity.insert(QStringLiteral("point:-text"), 1);

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
    arity.insert(QStringLiteral("point:-orientation"), 1);

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
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    parsesMapObjectAttributesAndOptions();
    keepsNegativeNumbersAsValues();
    keepsBracketedValuesTogether();
    return 0;
}
