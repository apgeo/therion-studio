#include "MapEditorObjectDetailsLogic.h"

#include "../../../core/CommandCatalogService.h"
#include "../../../core/TherionDocumentParser.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <cmath>

namespace TherionStudio
{
bool sourcePointsDifferForCommands(const QPointF &a, const QPointF &b)
{
    return (a - b).manhattanLength() > 0.01;
}

bool isConfigurableMapObjectKind(const QString &kind)
{
    const QString normalized = kind.trimmed().toLower();
    return normalized == QStringLiteral("scrap")
        || normalized == QStringLiteral("point")
        || normalized == QStringLiteral("line")
        || normalized == QStringLiteral("area");
}

QString objectKindForDirective(const QString &directiveToken)
{
    const QString directive = directiveToken.trimmed().toLower();
    if (directive == QStringLiteral("scrap")
        || directive == QStringLiteral("point")
        || directive == QStringLiteral("line")
        || directive == QStringLiteral("area")) {
        return directive;
    }
    return QString();
}

bool tokenLooksNumericForMapDetails(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }
    static const QRegularExpression numericPattern(
        QStringLiteral(R"(^[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][+-]?\d+)?$)"));
    return numericPattern.match(token).hasMatch();
}

bool isOrientationOptionTokenForMapDetails(const QString &token)
{
    const QString normalized = token.trimmed().toLower();
    return normalized == QStringLiteral("-orientation")
        || normalized == QStringLiteral("orientation")
        || normalized == QStringLiteral("-orient")
        || normalized == QStringLiteral("orient");
}

qreal normalizeOrientationDegreesForMapDetails(qreal value)
{
    qreal normalized = std::fmod(value, 360.0);
    if (normalized < 0.0) {
        normalized += 360.0;
    }
    if (normalized >= 360.0) {
        normalized -= 360.0;
    }
    return normalized;
}

QString normalizeSymbolTypeTokenForMapDetails(const QString &token)
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

QString lineTypeTokenForMapDetails(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("line")) {
        return QString();
    }
    return normalizeSymbolTypeTokenForMapDetails(parsedLine.tokens.value(1));
}

QString pointTypeTokenForMapDetails(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("point")) {
        return QString();
    }

    int numericCoordinateTokens = 0;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index).trimmed();
        if (token.isEmpty()) {
            continue;
        }
        if (token.startsWith(QLatin1Char('-')) && !tokenLooksNumericForMapDetails(token)) {
            break;
        }
        if (tokenLooksNumericForMapDetails(token)) {
            ++numericCoordinateTokens;
            continue;
        }
        if (numericCoordinateTokens < 2) {
            continue;
        }
        return normalizeSymbolTypeTokenForMapDetails(token);
    }

    return QString();
}

QSet<QString> parseOrientationRestrictedTypesFromText(const QString &text)
{
    QSet<QString> restrictedTypes;
    if (text.trimmed().isEmpty()) {
        return restrictedTypes;
    }

    static const QRegularExpression onlyWithTypePattern(
        QStringLiteral(R"(only[^.\n\r]{0,200}?\b([a-z][a-z0-9_-]*)\s+type\b)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator onlyWithMatches = onlyWithTypePattern.globalMatch(text);
    while (onlyWithMatches.hasNext()) {
        const QRegularExpressionMatch match = onlyWithMatches.next();
        const QString typeToken = normalizeSymbolTypeTokenForMapDetails(match.captured(1));
        if (!typeToken.isEmpty()) {
            restrictedTypes.insert(typeToken);
        }
    }

    static const QRegularExpression quotedSymbolTypePattern(
        QStringLiteral(R"((?:`|'|\|)([a-z][a-z0-9_-]*)(?:`|'|\|)\s+symbol\s+type\b)"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator quotedMatches = quotedSymbolTypePattern.globalMatch(text);
    while (quotedMatches.hasNext()) {
        const QRegularExpressionMatch match = quotedMatches.next();
        const QString typeToken = normalizeSymbolTypeTokenForMapDetails(match.captured(1));
        if (!typeToken.isEmpty()) {
            restrictedTypes.insert(typeToken);
        }
    }

    return restrictedTypes;
}

QHash<QString, QSet<QString>> loadOrientationTypeRestrictionsFromCatalog()
{
    QHash<QString, QSet<QString>> restrictionsByCommand;

    const QJsonObject catalogObject = CommandCatalogService::catalogObject();
    if (catalogObject.isEmpty()) {
        return restrictionsByCommand;
    }

    const QJsonArray commands = catalogObject.value(QStringLiteral("commands")).toArray();
    for (const QJsonValue &commandValue : commands) {
        const QJsonObject commandObject = commandValue.toObject();
        const QString commandName = commandObject.value(QStringLiteral("name")).toString().trimmed().toLower();
        if (commandName.isEmpty()) {
            continue;
        }

        const QJsonArray options = commandObject.value(QStringLiteral("options")).toArray();
        for (const QJsonValue &optionValue : options) {
            const QJsonObject optionObject = optionValue.toObject();
            const QString optionKey = optionObject.value(QStringLiteral("option_key")).toString().trimmed().toLower();
            if (optionKey != QStringLiteral("-orientation")) {
                continue;
            }

            QStringList metadataTexts;
            metadataTexts.append(optionObject.value(QStringLiteral("description")).toString());
            metadataTexts.append(optionObject.value(QStringLiteral("raw")).toString());
            metadataTexts.append(optionObject.value(QStringLiteral("name")).toString());
            metadataTexts.append(optionObject.value(QStringLiteral("signature")).toString());
            const QJsonArray dependencies = optionObject.value(QStringLiteral("dependencies")).toArray();
            for (const QJsonValue &dependencyValue : dependencies) {
                metadataTexts.append(dependencyValue.toString());
            }

            QSet<QString> restrictedTypes;
            for (const QString &metadataText : metadataTexts) {
                restrictedTypes.unite(parseOrientationRestrictedTypesFromText(metadataText));
            }

            restrictionsByCommand.insert(commandName, restrictedTypes);
            break;
        }
    }

    return restrictionsByCommand;
}

const QHash<QString, QSet<QString>> &orientationTypeRestrictionsByCommand()
{
    static const QHash<QString, QSet<QString>> restrictions = loadOrientationTypeRestrictionsFromCatalog();
    return restrictions;
}

bool isOrientationSupportedForParsedLine(const TherionParsedLine &parsedLine)
{
    const QString commandName = parsedLine.directive.trimmed().toLower();
    if (commandName != QStringLiteral("point") && commandName != QStringLiteral("line")) {
        return false;
    }

    const QHash<QString, QSet<QString>> &restrictionsByCommand = orientationTypeRestrictionsByCommand();
    if (!restrictionsByCommand.contains(commandName)) {
        return true;
    }

    const QSet<QString> restrictedTypes = restrictionsByCommand.value(commandName);
    if (restrictedTypes.isEmpty()) {
        return true;
    }

    const QString symbolType = commandName == QStringLiteral("point")
        ? pointTypeTokenForMapDetails(parsedLine)
        : lineTypeTokenForMapDetails(parsedLine);
    if (symbolType.isEmpty()) {
        return false;
    }

    return restrictedTypes.contains(symbolType);
}

bool isLinePointLeftSizeSupportedForParsedLine(const TherionParsedLine &parsedLine)
{
    return parsedLine.directive == QStringLiteral("line")
        && lineTypeTokenForMapDetails(parsedLine) == QStringLiteral("slope");
}

QVector<QPair<int, int>> coordinateTokenPairsForLine(const TherionParsedLine &parsedLine, int startTokenIndex)
{
    QVector<QPair<int, int>> pairs;
    QVector<int> numericIndices;
    numericIndices.reserve(parsedLine.tokens.size());

    const int firstTokenIndex = qMax(0, startTokenIndex);
    if (firstTokenIndex == 0) {
        int firstNonQuotedIndex = -1;
        for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
            if (index < parsedLine.tokenSpans.size()
                && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
                continue;
            }
            firstNonQuotedIndex = index;
            break;
        }

        if (firstNonQuotedIndex >= 0 && !tokenLooksNumericForMapDetails(parsedLine.tokens.at(firstNonQuotedIndex))) {
            return pairs;
        }
    }

    if (firstTokenIndex < parsedLine.tokens.size()) {
        const QString firstToken = parsedLine.tokens.at(firstTokenIndex);
        if (firstTokenIndex == 0
            && firstToken.startsWith(QLatin1Char('-'))
            && !tokenLooksNumericForMapDetails(firstToken)) {
            return pairs;
        }
    }

    bool sawCoordinateToken = false;
    bool skipOptionValueToken = false;
    for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
        if (skipOptionValueToken && !sawCoordinateToken) {
            skipOptionValueToken = false;
            continue;
        }

        const QString token = parsedLine.tokens.at(index);
        if (!sawCoordinateToken
            && firstTokenIndex > 0
            && token.startsWith(QLatin1Char('-'))
            && !tokenLooksNumericForMapDetails(token)) {
            if (index + 1 < parsedLine.tokens.size()) {
                const QString nextToken = parsedLine.tokens.at(index + 1);
                if (!nextToken.startsWith(QLatin1Char('-')) || tokenLooksNumericForMapDetails(nextToken)) {
                    skipOptionValueToken = true;
                }
            }
            continue;
        }

        const bool numeric = tokenLooksNumericForMapDetails(token);
        if (!numeric) {
            if (sawCoordinateToken) {
                break;
            }
            continue;
        }

        if (index < parsedLine.tokenSpans.size()
            && parsedLine.tokenSpans.at(index).type == TherionTokenType::QuotedString) {
            continue;
        }

        numericIndices.append(index);
        sawCoordinateToken = true;
    }

    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        pairs.append(qMakePair(numericIndices.at(index), numericIndices.at(index + 1)));
    }
    return pairs;
}

std::optional<qreal> pointOrientationFromParsedLine(const TherionParsedLine &parsedLine)
{
    std::optional<qreal> orientation;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (!isOrientationOptionTokenForMapDetails(parsedLine.tokens.at(index))) {
            continue;
        }
        if (index + 1 >= parsedLine.tokens.size()) {
            continue;
        }
        const QString valueToken = parsedLine.tokens.at(index + 1).trimmed();
        if (valueToken.startsWith(QLatin1Char('-')) && !tokenLooksNumericForMapDetails(valueToken)) {
            continue;
        }
        bool ok = false;
        const qreal value = valueToken.toDouble(&ok);
        if (!ok) {
            continue;
        }
        orientation = normalizeOrientationDegreesForMapDetails(value);
    }
    return orientation;
}

bool isLinePointOptionTokenForMapDetails(const QString &token, const QStringList &optionNames)
{
    const QString normalized = token.trimmed().toLower();
    for (const QString &optionName : optionNames) {
        if (normalized == optionName.trimmed().toLower()) {
            return true;
        }
    }
    return false;
}

std::optional<qreal> linePointNumericOptionForSourceVertex(const QString &documentText,
                                                           int lineNumber,
                                                           int sourceVertexIndex,
                                                           const QStringList &optionNames,
                                                           bool normalizeOrientation)
{
    if (lineNumber <= 0 || sourceVertexIndex < 0) {
        return std::nullopt;
    }

    QStringList lines = documentText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    if (lineNumber > lines.size()) {
        return std::nullopt;
    }

    const int blockStartLineIndex = lineNumber - 1;
    const TherionParsedLine startLine = TherionDocumentParser::parseLine(lines.at(blockStartLineIndex), lineNumber);
    if (startLine.directive != QStringLiteral("line")) {
        return std::nullopt;
    }

    int blockEndLineIndex = -1;
    for (int candidateIndex = blockStartLineIndex + 1; candidateIndex < lines.size(); ++candidateIndex) {
        const TherionParsedLine candidateLine = TherionDocumentParser::parseLine(lines.at(candidateIndex), candidateIndex + 1);
        if (candidateLine.directive == QStringLiteral("endline")) {
            blockEndLineIndex = candidateIndex;
            break;
        }
    }
    if (blockEndLineIndex < 0) {
        return std::nullopt;
    }

    int nextSourceIndex = 0;
    for (int rowIndex = blockStartLineIndex; rowIndex < blockEndLineIndex; ++rowIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(rowIndex), rowIndex + 1);
        const int startTokenIndex = rowIndex == blockStartLineIndex ? 1 : 0;
        const QVector<QPair<int, int>> coordinatePairs = coordinateTokenPairsForLine(parsedLine, startTokenIndex);
        if (coordinatePairs.isEmpty()) {
            continue;
        }
        const int localPairCount = coordinatePairs.size();

        const int firstSourceIndex = nextSourceIndex;
        nextSourceIndex += localPairCount;
        if (sourceVertexIndex < firstSourceIndex || sourceVertexIndex >= nextSourceIndex) {
            continue;
        }

        auto optionValueFromLine = [&](const TherionParsedLine &optionLine, int firstTokenIndex) -> std::optional<qreal> {
            std::optional<qreal> optionValue;
            for (int optionIndex = firstTokenIndex; optionIndex < optionLine.tokens.size(); ++optionIndex) {
                if (!isLinePointOptionTokenForMapDetails(optionLine.tokens.at(optionIndex), optionNames)) {
                    continue;
                }
                if (optionIndex + 1 >= optionLine.tokens.size()) {
                    continue;
                }
                const QString valueToken = optionLine.tokens.at(optionIndex + 1).trimmed();
                if (valueToken.startsWith(QLatin1Char('-')) && !tokenLooksNumericForMapDetails(valueToken)) {
                    continue;
                }
                bool ok = false;
                const qreal value = valueToken.toDouble(&ok);
                if (ok) {
                    optionValue = normalizeOrientation ? normalizeOrientationDegreesForMapDetails(value) : value;
                }
            }
            return optionValue;
        };

        std::optional<qreal> optionValue =
            optionValueFromLine(parsedLine, qMax(startTokenIndex, coordinatePairs.at(sourceVertexIndex - firstSourceIndex).second + 1));
        for (int optionRowIndex = rowIndex + 1; optionRowIndex < blockEndLineIndex; ++optionRowIndex) {
            const TherionParsedLine optionRowLine = TherionDocumentParser::parseLine(lines.at(optionRowIndex), optionRowIndex + 1);
            if (!coordinateTokenPairsForLine(optionRowLine, 0).isEmpty()) {
                break;
            }
            if (const std::optional<qreal> rowValue = optionValueFromLine(optionRowLine, 0)) {
                optionValue = rowValue;
            }
        }
        return optionValue;
    }

    return std::nullopt;
}

std::optional<qreal> linePointOrientationForSourceVertex(const QString &documentText,
                                                         int lineNumber,
                                                         int sourceVertexIndex)
{
    return linePointNumericOptionForSourceVertex(documentText,
                                                lineNumber,
                                                sourceVertexIndex,
                                                QStringList{QStringLiteral("-orientation"),
                                                            QStringLiteral("orientation"),
                                                            QStringLiteral("-orient"),
                                                            QStringLiteral("orient")},
                                                true);
}

std::optional<qreal> linePointLeftSizeForSourceVertex(const QString &documentText,
                                                      int lineNumber,
                                                      int sourceVertexIndex)
{
    std::optional<qreal> size = linePointNumericOptionForSourceVertex(documentText,
                                                                     lineNumber,
                                                                     sourceVertexIndex,
                                                                     QStringList{QStringLiteral("-size"),
                                                                                 QStringLiteral("size"),
                                                                                 QStringLiteral("-l-size"),
                                                                                 QStringLiteral("l-size")},
                                                                     false);
    if (size.has_value() && size.value() > 0.0) {
        return size;
    }
    return std::nullopt;
}

}
