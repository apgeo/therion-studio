#pragma once

#include <QRegularExpression>
#include <QString>

namespace TherionStudio::TherionTokenRules
{
enum class NumericTokenContext
{
    Plain,
    BracketedOptionValue,
    SyntaxToken
};

inline QString normalizedNumericToken(QString token, NumericTokenContext context = NumericTokenContext::Plain)
{
    token = token.trimmed();

    if (context == NumericTokenContext::BracketedOptionValue
        || context == NumericTokenContext::SyntaxToken) {
        while (token.startsWith(QLatin1Char('['))) {
            token.remove(0, 1);
        }
        while (token.endsWith(QLatin1Char(']'))) {
            token.chop(1);
        }
    }

    if (context == NumericTokenContext::SyntaxToken) {
        while (!token.isEmpty()
               && (token.startsWith(QLatin1Char('('))
                   || token.startsWith(QLatin1Char('{')))) {
            token.remove(0, 1);
        }
        while (!token.isEmpty()
               && (token.endsWith(QLatin1Char(')'))
                   || token.endsWith(QLatin1Char('}'))
                   || token.endsWith(QLatin1Char(','))
                   || token.endsWith(QLatin1Char(';')))) {
            token.chop(1);
        }
    }

    return token;
}

inline bool isNumericToken(const QString &token, NumericTokenContext context = NumericTokenContext::Plain)
{
    const QString normalized = normalizedNumericToken(token, context);
    if (normalized.isEmpty()) {
        return false;
    }

    static const QRegularExpression numericPattern(
        QStringLiteral(R"(^[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][+-]?\d+)?$)"));
    return numericPattern.match(normalized).hasMatch();
}

inline bool tokenStartsOption(const QString &token)
{
    const QString trimmed = token.trimmed();
    return trimmed.startsWith(QLatin1Char('-'))
        && !isNumericToken(trimmed, NumericTokenContext::BracketedOptionValue);
}
}
