#include "../src/core/TherionTokenRules.h"

#include <QCoreApplication>
#include <QString>

#include <cstdio>
#include <cstdlib>

using namespace TherionStudio;

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "TherionTokenRulesTest failed: %s\n", message);
        std::exit(1);
    }
}

void recognizesPlainNumericTokens()
{
    require(TherionTokenRules::isNumericToken(QStringLiteral("0")), "zero should be numeric");
    require(TherionTokenRules::isNumericToken(QStringLiteral("-45")), "negative integers should be numeric");
    require(TherionTokenRules::isNumericToken(QStringLiteral("+12.5")), "explicit positive decimals should be numeric");
    require(TherionTokenRules::isNumericToken(QStringLiteral(".25")), "leading-dot decimals should be numeric");
    require(TherionTokenRules::isNumericToken(QStringLiteral("-1.5e-2")), "exponent decimals should be numeric");
    require(!TherionTokenRules::isNumericToken(QStringLiteral("-orientation")), "options should not be numeric");
    require(!TherionTokenRules::isNumericToken(QStringLiteral("\"12\"")), "quoted numbers should not be plain numeric tokens");
}

void recognizesDecoratedNumericTokens()
{
    require(TherionTokenRules::isNumericToken(QStringLiteral("[-45]"),
                                              TherionTokenRules::NumericTokenContext::BracketedOptionValue),
            "bracketed negative option values should be numeric");
    require(TherionTokenRules::isNumericToken(QStringLiteral("(12.5),"),
                                              TherionTokenRules::NumericTokenContext::SyntaxToken),
            "syntax highlighter punctuation should not prevent numeric classification");
    require(!TherionTokenRules::isNumericToken(QStringLiteral("[station]"),
                                               TherionTokenRules::NumericTokenContext::BracketedOptionValue),
            "bracketed text should not be numeric");
}

void distinguishesOptionsFromNegativeValues()
{
    require(TherionTokenRules::tokenStartsOption(QStringLiteral("-orientation")),
            "named option tokens should start options");
    require(!TherionTokenRules::tokenStartsOption(QStringLiteral("-45")),
            "negative numeric values must not start options");
    require(!TherionTokenRules::tokenStartsOption(QStringLiteral("[-45]")),
            "bracketed negative numeric values must not start options");
    require(!TherionTokenRules::tokenStartsOption(QStringLiteral("station")),
            "plain values must not start options");
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    recognizesPlainNumericTokens();
    recognizesDecoratedNumericTokens();
    distinguishesOptionsFromNegativeValues();
    return 0;
}
