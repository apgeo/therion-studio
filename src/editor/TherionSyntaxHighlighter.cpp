#include "TherionSyntaxHighlighter.h"

#include "../core/TherionCommandLineModel.h"
#include "../core/TherionDocumentParser.h"
#include "../core/TherionTokenRules.h"

#include <QColor>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QTextCharFormat>

#include <utility>

namespace TherionStudio
{
QStringList extractOptionKeysFromSignatureAliases(const QString &signature);

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

bool applicationUsesDarkSyntaxPalette()
{
    const QColor baseColor = QGuiApplication::palette().color(QPalette::Base);
    return baseColor.isValid() && baseColor.lightnessF() < 0.5;
}

QSet<QString> extractOptionKeys(const QString &optionKeyField)
{
    QSet<QString> keys;
    const QStringList segments = optionKeyField.split(QRegularExpression(QStringLiteral("[,/]")),
                                                      Qt::SkipEmptyParts);
    static const QRegularExpression keyPattern(QStringLiteral("^-[A-Za-z][A-Za-z0-9-]*$"));
    for (QString segment : segments) {
        segment = segment.trimmed().toLower();
        if (segment.isEmpty() || !segment.startsWith(QLatin1Char('-'))) {
            continue;
        }
        if (segment.contains(QLatin1Char('<')) || segment.contains(QLatin1Char('>'))) {
            continue;
        }
        if (!keyPattern.match(segment).hasMatch()) {
            continue;
        }
        keys.insert(segment);
    }
    return keys;
}

QSet<QString> extractOptionKeys(const QString &optionKeyField, const QString &signature)
{
    QSet<QString> keys = extractOptionKeys(optionKeyField);
    for (const QString &aliasKey : extractOptionKeysFromSignatureAliases(signature)) {
        keys.insert(aliasKey.trimmed().toLower());
    }
    return keys;
}

bool looksLikeOptionToken(const QString &token)
{
    static const QRegularExpression optionPattern(QStringLiteral("^-[A-Za-z][A-Za-z0-9-]*$"));
    return optionPattern.match(token.trimmed()).hasMatch();
}

QString normalizeSymbolTypeToken(const QString &token)
{
    QString normalized = token.trimmed().toLower();
    if (normalized.isEmpty() || normalized.startsWith(QLatin1Char('-'))) {
        return QString();
    }

    const int separatorIndex = normalized.indexOf(QLatin1Char(':'));
    if (separatorIndex > 0) {
        normalized = normalized.left(separatorIndex).trimmed();
    }
    return normalized;
}

QString symbolTypeForSubtypeLookup(const QString &commandName, const TherionParsedLine &parsedLine)
{
    const QString normalizedCommand = commandName.trimmed().toLower();
    if (normalizedCommand == QStringLiteral("line")
        || normalizedCommand == QStringLiteral("area")) {
        for (int tokenIndex = 1; tokenIndex < parsedLine.tokens.size(); ++tokenIndex) {
            const QString token = parsedLine.tokens.at(tokenIndex).trimmed();
            if (token.startsWith(QLatin1Char('-'))) {
                break;
            }
            const QString normalizedType = normalizeSymbolTypeToken(token);
            if (!normalizedType.isEmpty()) {
                return normalizedType;
            }
        }
        return QString();
    }

    if (normalizedCommand == QStringLiteral("point")) {
        int positionalToken = 0;
        for (int tokenIndex = 1; tokenIndex < parsedLine.tokens.size(); ++tokenIndex) {
            const QString token = parsedLine.tokens.at(tokenIndex).trimmed();
            if (token.isEmpty()) {
                continue;
            }
            if (token.startsWith(QLatin1Char('-'))) {
                break;
            }
            ++positionalToken;
            if (positionalToken == 3) {
                return normalizeSymbolTypeToken(token);
            }
        }
    }

    return QString();
}

QSet<QString> lowerSetFromArray(const QJsonArray &values)
{
    QSet<QString> lowered;
    for (const QJsonValue &value : values) {
        const QString normalized = value.toString().trimmed().toLower();
        if (!normalized.isEmpty()) {
            lowered.insert(normalized);
        }
    }
    return lowered;
}

QSet<QString> extractClosingDirectiveTokens(const QJsonArray &syntaxArray)
{
    QSet<QString> closingTokens;
    static const QRegularExpression closingPattern(QStringLiteral("\\bend[a-z][a-z0-9-]*\\b"),
                                                   QRegularExpression::CaseInsensitiveOption);
    for (const QJsonValue &syntaxValue : syntaxArray) {
        const QString syntaxText = syntaxValue.toString();
        auto matchIterator = closingPattern.globalMatch(syntaxText);
        while (matchIterator.hasNext()) {
            const QRegularExpressionMatch match = matchIterator.next();
            const QString token = match.captured(0).trimmed().toLower();
            if (!token.isEmpty()) {
                closingTokens.insert(token);
            }
        }
    }
    return closingTokens;
}

QSet<QString> extractClosingDirectiveIdTokens(const QJsonArray &syntaxArray)
{
    QSet<QString> closingIdTokens;
    static const QRegularExpression closingWithIdPattern(
        QStringLiteral("\\b(end[a-z][a-z0-9-]*)\\s+\\[<[^>]*\\bid\\b[^>]*>\\]"),
        QRegularExpression::CaseInsensitiveOption);
    for (const QJsonValue &syntaxValue : syntaxArray) {
        const QString syntaxText = syntaxValue.toString();
        auto matchIterator = closingWithIdPattern.globalMatch(syntaxText);
        while (matchIterator.hasNext()) {
            const QRegularExpressionMatch match = matchIterator.next();
            const QString token = match.captured(1).trimmed().toLower();
            if (!token.isEmpty()) {
                closingIdTokens.insert(token);
            }
        }
    }
    return closingIdTokens;
}

bool textContainsIdPlaceholder(const QString &text)
{
    static const QRegularExpression idPlaceholderPattern(
        QStringLiteral("<[^>]*\\bid\\b[^>]*>"),
        QRegularExpression::CaseInsensitiveOption);
    return idPlaceholderPattern.match(text).hasMatch();
}
}

TherionSyntaxHighlighter::TherionSyntaxHighlighter(CommandCatalogStore catalogStore, QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , catalogStore_(std::move(catalogStore))
{
    loadPalette();
}

void TherionSyntaxHighlighter::reloadPaletteForApplicationAppearance()
{
    loadPalette();
    rehighlight();
}

void TherionSyntaxHighlighter::loadPalette()
{
    keywordTokens_.clear();
    commandTokens_.clear();
    commandOptionTokens_.clear();
    commandAllowedValues_.clear();
    commandOptionValueArity_.clear();
    commandOptionEnumValues_.clear();
    commandSubtypeByTypeTokens_.clear();
    commandPositionalIdTokenIndexes_.clear();
    commandOptionIdValueTokens_.clear();
    closingDirectiveIdTokens_.clear();

    const bool darkPalette = applicationUsesDarkSyntaxPalette();
    baseTextFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#D4D4D4")) : QColor(QStringLiteral("#24292f")));
    keywordFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#569CD6")) : QColor(QStringLiteral("#0550ae")), true);
    optionFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#9CDCFE")) : QColor(QStringLiteral("#0969da")));
    identifierFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#DCDCAA")) : QColor(QStringLiteral("#8250df")));
    invalidTokenFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#F44747")) : QColor(QStringLiteral("#cf222e")));
    invalidTokenFormat_.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    invalidTokenFormat_.setUnderlineColor(invalidTokenFormat_.foreground().color());
    stringFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#CE9178")) : QColor(QStringLiteral("#0a3069")));
    numberFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#B5CEA8")) : QColor(QStringLiteral("#116329")));
    commentFormat_ = makeFormat(darkPalette ? QColor(QStringLiteral("#6A9955")) : QColor(QStringLiteral("#6e7781")), false, true);

    QFile paletteFile(QStringLiteral(":/resources/therion_syntax_palette.json"));
    if (!paletteFile.open(QIODevice::ReadOnly)) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(paletteFile.readAll());
    if (!document.isObject()) {
        return;
    }

    const QJsonObject rootObject = document.object();
    QJsonObject stylesObject = rootObject.value(darkPalette ? QStringLiteral("darkStyles") : QStringLiteral("styles")).toObject();
    if (stylesObject.isEmpty()) {
        stylesObject = rootObject.value(QStringLiteral("styles")).toObject();
    }

    applyStyle(QStringLiteral("baseText"), stylesObject.value(QStringLiteral("baseText")).toObject());
    applyStyle(QStringLiteral("keyword"), stylesObject.value(QStringLiteral("keyword")).toObject());
    applyStyle(QStringLiteral("option"), stylesObject.value(QStringLiteral("option")).toObject());
    applyStyle(QStringLiteral("identifier"), stylesObject.value(QStringLiteral("identifier")).toObject());
    applyStyle(QStringLiteral("invalidToken"), stylesObject.value(QStringLiteral("invalidToken")).toObject());
    applyStyle(QStringLiteral("string"), stylesObject.value(QStringLiteral("string")).toObject());
    applyStyle(QStringLiteral("number"), stylesObject.value(QStringLiteral("number")).toObject());
    applyStyle(QStringLiteral("comment"), stylesObject.value(QStringLiteral("comment")).toObject());

    const QJsonArray keywordsArray = rootObject.value(QStringLiteral("additionalKeywords")).toArray();
    for (const QJsonValue &value : keywordsArray) {
        const QString keyword = value.toString().trimmed();
        if (keyword.isEmpty()) {
            continue;
        }

        keywordTokens_.insert(keyword.toLower());
    }

    loadCommandCatalogKeywords();
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
    } else if (styleName == QStringLiteral("option")) {
        optionFormat_ = format;
    } else if (styleName == QStringLiteral("identifier")) {
        identifierFormat_ = format;
    } else if (styleName == QStringLiteral("invalidToken")) {
        invalidTokenFormat_ = format;
        invalidTokenFormat_.setUnderlineStyle(QTextCharFormat::WaveUnderline);
        invalidTokenFormat_.setUnderlineColor(format.foreground().color());
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
    const QString directive = parsedLine.directive.trimmed().toLower();
    const bool knownCommand = commandTokens_.contains(directive);
    const QSet<QString> commandAllowedValues = commandAllowedValues_.value(directive);
    const QSet<QString> commandOptions = commandOptionTokens_.value(directive);
    const QHash<QString, QString> optionValueArity = commandOptionValueArity_.value(directive);
    const QHash<QString, QSet<QString>> optionEnumValues = commandOptionEnumValues_.value(directive);
    const QHash<QString, QSet<QString>> subtypeByTypeValues = commandSubtypeByTypeTokens_.value(directive);
    const QSet<int> idPositionalIndexes = commandPositionalIdTokenIndexes_.value(directive);
    const QSet<QString> idOptionTokens = commandOptionIdValueTokens_.value(directive);
    const bool closingDirectiveAllowsId = closingDirectiveIdTokens_.contains(directive);
    const QString symbolTypeToken = symbolTypeForSubtypeLookup(directive, parsedLine);

    QString activeOptionToken;
    QString activeOptionArity;
    QSet<QString> activeAllowedValues;
    bool activeValidateValues = false;
    bool activeOptionExpectsId = false;

    int tokenIndex = 0;
    for (int index = 0; index < parsedLine.tokenSpans.size(); ++index) {
        const TherionParsedToken &tokenSpan = parsedLine.tokenSpans.at(index);
        if (tokenSpan.type == TherionTokenType::Comment) {
            setFormat(tokenSpan.start, tokenSpan.length, commentFormat_);
            continue;
        }

        const QString normalizedToken = tokenSpan.text.trimmed().toLower();
        const bool optionLikeToken = looksLikeOptionToken(normalizedToken);
        const bool embeddedOptionValueToken = commandTokenEmbedsOptionValue(normalizedToken);
        bool identifierToken = false;
        if (knownCommand && tokenIndex > 0 && !optionLikeToken && idPositionalIndexes.contains(tokenIndex)) {
            identifierToken = true;
        }
        if (!knownCommand && closingDirectiveAllowsId && tokenIndex == 1 && !optionLikeToken) {
            identifierToken = true;
        }
        bool formatted = false;

        if (knownCommand
            && tokenIndex > 0
            && embeddedOptionValueToken
            && (activeOptionToken.isEmpty() || activeOptionToken == QStringLiteral("-subtype"))) {
            setFormat(tokenSpan.start, tokenSpan.length, invalidTokenFormat_);
            formatted = true;
            activeOptionToken.clear();
            activeOptionArity.clear();
            activeAllowedValues.clear();
            activeValidateValues = false;
            activeOptionExpectsId = false;
        } else if (tokenIndex == 0 && keywordTokens_.contains(normalizedToken)) {
            setFormat(tokenSpan.start, tokenSpan.length, keywordFormat_);
            formatted = true;
        } else if (knownCommand && tokenIndex > 0 && optionLikeToken) {
            if (commandOptions.contains(normalizedToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, optionFormat_);
                formatted = true;

                activeOptionToken = normalizedToken;
                activeOptionArity = optionValueArity.value(normalizedToken).trimmed().toUpper();
                activeAllowedValues = optionEnumValues.value(normalizedToken);
                activeValidateValues = !activeAllowedValues.isEmpty();
                activeOptionExpectsId = idOptionTokens.contains(normalizedToken);

                if (activeOptionToken == QStringLiteral("-subtype") && !symbolTypeToken.isEmpty()) {
                    const QSet<QString> subtypeValues = subtypeByTypeValues.value(symbolTypeToken);
                    if (subtypeValues.contains(QStringLiteral("*"))) {
                        activeAllowedValues.clear();
                        activeValidateValues = false;
                    } else if (!subtypeValues.isEmpty()) {
                        activeAllowedValues = subtypeValues;
                        activeValidateValues = true;
                    }
                }

                if (activeOptionArity == QStringLiteral("0")) {
                    activeOptionToken.clear();
                    activeOptionArity.clear();
                    activeAllowedValues.clear();
                    activeValidateValues = false;
                    activeOptionExpectsId = false;
                }
            } else {
                setFormat(tokenSpan.start, tokenSpan.length, invalidTokenFormat_);
                formatted = true;
                activeOptionToken.clear();
                activeOptionArity.clear();
                activeAllowedValues.clear();
                activeValidateValues = false;
                activeOptionExpectsId = false;
            }
        } else {
            bool invalidValue = false;
            if (knownCommand && !activeOptionToken.isEmpty()) {
                if (activeValidateValues && !activeAllowedValues.contains(normalizedToken)) {
                    invalidValue = true;
                }
                if (!invalidValue && activeOptionExpectsId) {
                    identifierToken = true;
                }

                if (activeOptionArity != QStringLiteral("N")) {
                    activeOptionToken.clear();
                    activeOptionArity.clear();
                    activeAllowedValues.clear();
                    activeValidateValues = false;
                    activeOptionExpectsId = false;
                }
            }

            if (invalidValue) {
                setFormat(tokenSpan.start, tokenSpan.length, invalidTokenFormat_);
                formatted = true;
            }
        }

        if (!formatted) {
            if (tokenSpan.type == TherionTokenType::QuotedString) {
                setFormat(tokenSpan.start, tokenSpan.length, stringFormat_);
            } else if (identifierToken) {
                setFormat(tokenSpan.start, tokenSpan.length, identifierFormat_);
            } else if (knownCommand && tokenIndex > 0 && commandAllowedValues.contains(normalizedToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, optionFormat_);
            } else if (keywordTokens_.contains(normalizedToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, keywordFormat_);
            } else if (TherionTokenRules::isNumericToken(tokenSpan.text,
                                                         TherionTokenRules::NumericTokenContext::SyntaxToken)) {
                setFormat(tokenSpan.start, tokenSpan.length, numberFormat_);
            }
        }

        ++tokenIndex;
    }
}

void TherionSyntaxHighlighter::loadCommandCatalogKeywords()
{
    const QJsonObject catalogObject = catalogStore_.catalogObject();
    if (catalogObject.isEmpty()) {
        return;
    }

    const QJsonArray commandsArray = catalogObject.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commandsArray) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        const QSet<QString> commandAllowedValues = lowerSetFromArray(commandObject.value(QStringLiteral("allowed_values")).toArray());
        if (!commandName.isEmpty()) {
            commandTokens_.insert(commandName);
            keywordTokens_.insert(commandName);
            if (!commandAllowedValues.isEmpty()) {
                commandAllowedValues_.insert(commandName, commandAllowedValues);
            }
        }

        const QSet<QString> closingDirectiveTokens = extractClosingDirectiveTokens(commandObject.value(QStringLiteral("syntax")).toArray());
        const QSet<QString> closingDirectiveIdTokens = extractClosingDirectiveIdTokens(commandObject.value(QStringLiteral("syntax")).toArray());
        keywordTokens_.unite(closingDirectiveTokens);
        closingDirectiveIdTokens_.unite(closingDirectiveIdTokens);

        QSet<QString> optionTokens;
        QHash<QString, QString> optionArities;
        QHash<QString, QSet<QString>> optionEnumValues;
        QSet<QString> optionIdTokens;
        const QJsonArray optionsArray = commandObject.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &optionValue : optionsArray) {
            const QJsonObject optionObject = optionValue.toObject();
            const QString optionKeyField = optionObject.value(QStringLiteral("option_key")).toString().trimmed();
            const QString signature = optionObject.value(QStringLiteral("signature")).toString().trimmed();
            const QSet<QString> optionKeys = extractOptionKeys(optionKeyField, signature);
            if (optionKeys.isEmpty()) {
                continue;
            }

            optionTokens.unite(optionKeys);
            const QString arity = optionObject.value(QStringLiteral("value_arity")).toString().trimmed().toUpper();
            const QSet<QString> allowedValues = lowerSetFromArray(optionObject.value(QStringLiteral("allowed_values")).toArray());
            const bool optionExpectsId = optionKeys.contains(QStringLiteral("-id"))
                || textContainsIdPlaceholder(optionObject.value(QStringLiteral("signature")).toString())
                || textContainsIdPlaceholder(optionObject.value(QStringLiteral("name")).toString())
                || textContainsIdPlaceholder(optionObject.value(QStringLiteral("raw")).toString());
            for (const QString &optionKey : optionKeys) {
                if (!arity.isEmpty()) {
                    optionArities.insert(optionKey, arity);
                }
                if (!allowedValues.isEmpty()) {
                    optionEnumValues.insert(optionKey, allowedValues);
                }
                if (optionExpectsId) {
                    optionIdTokens.insert(optionKey);
                }
            }
        }

        QSet<int> positionalIdIndexes;
        int positionalIndex = 1;
        const QJsonArray argumentsArray = commandObject.value(QStringLiteral("arguments")).toArray();
        for (const QJsonValue &argumentValue : argumentsArray) {
            const QJsonObject argumentObject = argumentValue.toObject();
            if (textContainsIdPlaceholder(argumentObject.value(QStringLiteral("signature")).toString())
                || textContainsIdPlaceholder(argumentObject.value(QStringLiteral("name")).toString())
                || textContainsIdPlaceholder(argumentObject.value(QStringLiteral("raw")).toString())) {
                positionalIdIndexes.insert(positionalIndex);
            }
            ++positionalIndex;
        }

        QHash<QString, QSet<QString>> subtypeByTypeValues;
        const QJsonObject subtypeByTypeObject = commandObject.value(QStringLiteral("subtype_by_type")).toObject();
        for (auto subtypeIterator = subtypeByTypeObject.begin(); subtypeIterator != subtypeByTypeObject.end(); ++subtypeIterator) {
            const QString typeToken = subtypeIterator.key().trimmed().toLower();
            if (typeToken.isEmpty()) {
                continue;
            }
            subtypeByTypeValues.insert(typeToken, lowerSetFromArray(subtypeIterator.value().toArray()));
        }

        if (!commandName.isEmpty()) {
            commandOptionTokens_.insert(commandName, optionTokens);
            commandOptionValueArity_.insert(commandName, optionArities);
            commandOptionEnumValues_.insert(commandName, optionEnumValues);
            commandSubtypeByTypeTokens_.insert(commandName, subtypeByTypeValues);
            commandPositionalIdTokenIndexes_.insert(commandName, positionalIdIndexes);
            commandOptionIdValueTokens_.insert(commandName, optionIdTokens);
        }

        const QJsonArray aliasesArray = commandObject.value(QStringLiteral("aliases")).toArray();
        for (const QJsonValue &aliasValue : aliasesArray) {
            const QString alias = aliasValue.toString().trimmed().toLower();
            if (!alias.isEmpty()) {
                commandTokens_.insert(alias);
                keywordTokens_.insert(alias);
                for (const QString &closingToken : closingDirectiveTokens) {
                    keywordTokens_.insert(closingToken);
                    if (!commandName.isEmpty()) {
                        const QString commandClosingPrefix = QStringLiteral("end") + commandName;
                        if (closingToken == commandClosingPrefix) {
                            const QString aliasClosingToken = QStringLiteral("end") + alias;
                            keywordTokens_.insert(aliasClosingToken);
                            if (closingDirectiveIdTokens.contains(closingToken)) {
                                closingDirectiveIdTokens_.insert(aliasClosingToken);
                            }
                        }
                    }
                }
                if (!commandAllowedValues.isEmpty()) {
                    commandAllowedValues_.insert(alias, commandAllowedValues);
                }
                commandOptionTokens_.insert(alias, optionTokens);
                commandOptionValueArity_.insert(alias, optionArities);
                commandOptionEnumValues_.insert(alias, optionEnumValues);
                commandSubtypeByTypeTokens_.insert(alias, subtypeByTypeValues);
                commandPositionalIdTokenIndexes_.insert(alias, positionalIdIndexes);
                commandOptionIdValueTokens_.insert(alias, optionIdTokens);
            }
        }
    }
}
}
