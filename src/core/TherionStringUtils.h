#pragma once

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
    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    return lines;
}

inline QStringList splitLinesNormalizingLineEndings(const QString &contents)
{
    QString normalized = contents;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return normalized.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
}
}
