#pragma once

#include "TherionSourceText.h"

#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
inline QStringList tokenizeWhitespace(const QString &text)
{
    return text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

inline QStringList splitLinesTrimmingCarriageReturns(const QString &contents)
{
    return TherionSourceText::splitTextLines(contents);
}

inline QStringList splitLinesNormalizingLineEndings(const QString &contents)
{
    QString normalized = contents;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return normalized.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
}
}
