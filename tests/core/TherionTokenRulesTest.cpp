#include "../../src/core/TherionTokenRules.h"

#include <QString>
#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class TherionTokenRulesTest : public QObject
{
    Q_OBJECT

private slots:
    void recognizesPlainNumericTokens();
    void recognizesDecoratedNumericTokens();
    void distinguishesOptionsFromNegativeValues();
};

void TherionTokenRulesTest::recognizesPlainNumericTokens()
{
    QVERIFY2(TherionTokenRules::isNumericToken(QStringLiteral("0")), "zero should be numeric");
    QVERIFY2(TherionTokenRules::isNumericToken(QStringLiteral("-45")), "negative integers should be numeric");
    QVERIFY2(TherionTokenRules::isNumericToken(QStringLiteral("+12.5")), "explicit positive decimals should be numeric");
    QVERIFY2(TherionTokenRules::isNumericToken(QStringLiteral(".25")), "leading-dot decimals should be numeric");
    QVERIFY2(TherionTokenRules::isNumericToken(QStringLiteral("-1.5e-2")), "exponent decimals should be numeric");
    QVERIFY2(!TherionTokenRules::isNumericToken(QStringLiteral("-orientation")), "options should not be numeric");
    QVERIFY2(!TherionTokenRules::isNumericToken(QStringLiteral("\"12\"")), "quoted numbers should not be plain numeric tokens");
}

void TherionTokenRulesTest::recognizesDecoratedNumericTokens()
{
    QVERIFY2(TherionTokenRules::isNumericToken(QStringLiteral("[-45]"),
                                               TherionTokenRules::NumericTokenContext::BracketedOptionValue),
             "bracketed negative option values should be numeric");
    QVERIFY2(TherionTokenRules::isNumericToken(QStringLiteral("(12.5),"),
                                               TherionTokenRules::NumericTokenContext::SyntaxToken),
             "syntax highlighter punctuation should not prevent numeric classification");
    QVERIFY2(!TherionTokenRules::isNumericToken(QStringLiteral("[station]"),
                                                TherionTokenRules::NumericTokenContext::BracketedOptionValue),
             "bracketed text should not be numeric");
}

void TherionTokenRulesTest::distinguishesOptionsFromNegativeValues()
{
    QVERIFY2(TherionTokenRules::tokenStartsOption(QStringLiteral("-orientation")),
             "named option tokens should start options");
    QVERIFY2(!TherionTokenRules::tokenStartsOption(QStringLiteral("-45")),
             "negative numeric values must not start options");
    QVERIFY2(!TherionTokenRules::tokenStartsOption(QStringLiteral("[-45]")),
             "bracketed negative numeric values must not start options");
    QVERIFY2(!TherionTokenRules::tokenStartsOption(QStringLiteral("station")),
             "plain values must not start options");
}
}

int runTherionTokenRulesTest(int argc, char **argv)
{
    TherionTokenRulesTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "TherionTokenRulesTest.moc"
