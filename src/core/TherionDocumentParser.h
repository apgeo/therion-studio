#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace TherionStudio
{
enum class TherionTokenType
{
    Other,
    QuotedString,
    Comment
};

struct TherionParsedToken
{
    int start = 0;
    int length = 0;
    QString text;
    TherionTokenType type = TherionTokenType::Other;
};

struct TherionParsedLine
{
    int lineNumber = 0;
    QString rawText;
    QString directive;
    QStringList tokens;
    QVector<TherionParsedToken> tokenSpans;
    int commentStart = -1;
    QString commentText;
};

class TherionDocumentParser final
{
public:
    static TherionParsedLine parseLine(const QString &line, int lineNumber = 0);
    static QVector<TherionParsedLine> parseText(const QString &text);
    static QStringList tokenizeLine(const QString &line);
};
}
