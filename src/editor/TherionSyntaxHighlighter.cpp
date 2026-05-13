#include "TherionSyntaxHighlighter.h"

#include "../core/TherionDocumentParser.h"

#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFont>
#include <QTextCharFormat>

namespace TherionStudio
{
namespace
{
QTextCharFormat makeFormat(const QColor &foreground, bool bold = false, bool italic = false)
{
    QTextCharFormat format;
    format.setForeground(foreground);
    format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(italic);
    return format;
}
}

TherionSyntaxHighlighter::TherionSyntaxHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    loadPalette();
}

void TherionSyntaxHighlighter::loadPalette()
{
    baseTextFormat_ = makeFormat(QColor(QStringLiteral("#D4D4D4")));
    keywordFormat_ = makeFormat(QColor(QStringLiteral("#569CD6")), true);
    stringFormat_ = makeFormat(QColor(QStringLiteral("#CE9178")));
    numberFormat_ = makeFormat(QColor(QStringLiteral("#B5CEA8")));
    commentFormat_ = makeFormat(QColor(QStringLiteral("#6A9955")), false, true);

    QFile paletteFile(QStringLiteral(":/resources/therion_syntax_palette.json"));
    if (!paletteFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(paletteFile.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject rootObject = document.object();
    const QJsonObject stylesObject = rootObject.value(QStringLiteral("styles")).toObject();

    applyStyle(QStringLiteral("baseText"), stylesObject.value(QStringLiteral("baseText")).toObject());
    applyStyle(QStringLiteral("keyword"), stylesObject.value(QStringLiteral("keyword")).toObject());
    applyStyle(QStringLiteral("string"), stylesObject.value(QStringLiteral("string")).toObject());
    applyStyle(QStringLiteral("number"), stylesObject.value(QStringLiteral("number")).toObject());
    applyStyle(QStringLiteral("comment"), stylesObject.value(QStringLiteral("comment")).toObject());

    const QJsonArray keywordsArray = rootObject.value(QStringLiteral("keywords")).toArray();
    for (const QJsonValue &value : keywordsArray) {
        const QString keyword = value.toString().trimmed();
        if (keyword.isEmpty()) {
            continue;
        }

        keywordTokens_.insert(keyword.toLower());
    }
}

void TherionSyntaxHighlighter::applyStyle(const QString &styleName, const QJsonObject &styleObject)
{
    const QColor foreground(styleObject.value(QStringLiteral("foreground")).toString());
    const bool bold = styleObject.value(QStringLiteral("bold")).toBool(false);
    const bool italic = styleObject.value(QStringLiteral("italic")).toBool(false);
    const QTextCharFormat format = makeFormat(foreground.isValid() ? foreground : QColor(QStringLiteral("#D4D4D4")), bold, italic);

    if (styleName == QStringLiteral("baseText")) {
        baseTextFormat_ = format;
    } else if (styleName == QStringLiteral("keyword")) {
        keywordFormat_ = format;
    } else if (styleName == QStringLiteral("string")) {
        stringFormat_ = format;
    } else if (styleName == QStringLiteral("number")) {
        numberFormat_ = format;
    } else if (styleName == QStringLiteral("comment")) {
        commentFormat_ = format;
    }
}

void TherionSyntaxHighlighter::highlightBlock(const QString &text)
{
    setFormat(0, text.length(), baseTextFormat_);

    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(text);
    for (int index = 0; index < parsedLine.tokenSpans.size(); ++index) {
        const TherionParsedToken &tokenSpan = parsedLine.tokenSpans.at(index);

        switch (tokenSpan.type) {
        case TherionTokenType::QuotedString:
            setFormat(tokenSpan.start, tokenSpan.length, stringFormat_);
            break;
        case TherionTokenType::Comment:
            setFormat(tokenSpan.start, tokenSpan.length, commentFormat_);
            break;
        case TherionTokenType::Other:
            if (keywordTokens_.contains(tokenSpan.text.toLower())) {
                setFormat(tokenSpan.start, tokenSpan.length, keywordFormat_);
            } else if (!tokenSpan.text.isEmpty()) {
                bool isNumber = true;
                bool sawDigit = false;
                bool sawDecimalPoint = false;
                int characterIndex = 0;

                if (tokenSpan.text.at(characterIndex) == QLatin1Char('+') || tokenSpan.text.at(characterIndex) == QLatin1Char('-')) {
                    ++characterIndex;
                }

                for (; characterIndex < tokenSpan.text.length(); ++characterIndex) {
                    const QChar character = tokenSpan.text.at(characterIndex);
                    if (character.isDigit()) {
                        sawDigit = true;
                        continue;
                    }
                    if (character == QLatin1Char('.') && !sawDecimalPoint) {
                        sawDecimalPoint = true;
                        continue;
                    }

                    isNumber = false;
                    break;
                }

                if (isNumber && sawDigit) {
                    setFormat(tokenSpan.start, tokenSpan.length, numberFormat_);
                }
            }
            break;
        }
    }
}
}
