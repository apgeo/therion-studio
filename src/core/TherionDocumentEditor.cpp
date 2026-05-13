#include "TherionDocumentEditor.h"

#include "TherionDocumentParser.h"

#include <QFileInfo>
#include <QPair>
#include <QSet>
#include <QRegularExpression>

namespace TherionStudio
{
namespace
{
bool tokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    int index = 0;
    if (token.at(index) == QLatin1Char('+') || token.at(index) == QLatin1Char('-')) {
        ++index;
    }

    bool sawDigit = false;
    bool sawDecimalPoint = false;
    for (; index < token.size(); ++index) {
        const QChar character = token.at(index);
        if (character.isDigit()) {
            sawDigit = true;
            continue;
        }

        if (character == QLatin1Char('.') && !sawDecimalPoint) {
            sawDecimalPoint = true;
            continue;
        }

        return false;
    }

    return sawDigit;
}

int optionValueTokenIndex(const TherionParsedLine &parsedLine, const QString &option)
{
    const QString normalizedOption = option.toLower();
    for (int index = 0; index + 1 < parsedLine.tokens.size(); ++index) {
        if (parsedLine.tokens.at(index).toLower() != normalizedOption) {
            continue;
        }

        const QString candidate = parsedLine.tokens.at(index + 1);
        if (!candidate.startsWith(QLatin1Char('-'))) {
            return index + 1;
        }
    }

    return -1;
}

int pointTypeTokenIndex(const TherionParsedLine &parsedLine)
{
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (token.startsWith(QLatin1Char('-')) || tokenLooksNumeric(token)) {
            continue;
        }

        return index;
    }

    return -1;
}

int nameTokenIndexForCategory(const QString &category, const TherionParsedLine &parsedLine)
{
    const QString normalizedCategory = category.trimmed().toLower();
    const QString directive = parsedLine.directive;

    if (normalizedCategory == QStringLiteral("stations")) {
        if (directive == QStringLiteral("point")) {
            const int stationNameIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-name"));
            if (stationNameIndex >= 0) {
                return stationNameIndex;
            }

            const int pointTypeIndex = pointTypeTokenIndex(parsedLine);
            if (pointTypeIndex >= 0 && parsedLine.tokens.value(pointTypeIndex).toLower() == QStringLiteral("station")) {
                const int stationIdentifierIndex = pointTypeIndex + 1;
                if (stationIdentifierIndex < parsedLine.tokens.size()) {
                    const QString token = parsedLine.tokens.at(stationIdentifierIndex);
                    if (!token.startsWith(QLatin1Char('-'))) {
                        return stationIdentifierIndex;
                    }
                }

                return pointTypeIndex;
            }
        }

        if (directive == QStringLiteral("station") && parsedLine.tokens.size() >= 2) {
            return 1;
        }
    }

    if (normalizedCategory == QStringLiteral("lines") || normalizedCategory == QStringLiteral("areas")) {
        const int identifierIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-id"));
        if (identifierIndex >= 0) {
            return identifierIndex;
        }

        return parsedLine.tokens.size() >= 2 ? 1 : -1;
    }

    if (normalizedCategory == QStringLiteral("points")) {
        const int labelIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-name"));
        if (labelIndex >= 0) {
            return labelIndex;
        }

        const int identifierIndex = optionValueTokenIndex(parsedLine, QStringLiteral("-id"));
        if (identifierIndex >= 0) {
            return identifierIndex;
        }

        const int pointTypeIndex = pointTypeTokenIndex(parsedLine);
        if (pointTypeIndex >= 0) {
            return pointTypeIndex;
        }
    }

    if (normalizedCategory == QStringLiteral("surveys")
        || normalizedCategory == QStringLiteral("maps")
        || normalizedCategory == QStringLiteral("scraps")) {
        return 1;
    }

    return -1;
}

QString quoteToken(const QString &token, QChar quoteCharacter)
{
    QString quotedToken = token;
    quotedToken.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    quotedToken.replace(quoteCharacter, QStringLiteral("\\") + quoteCharacter);
    return QString(quoteCharacter) + quotedToken + QString(quoteCharacter);
}

QString replacementTokenForLine(const QString &newName, const QString &lineText, const TherionParsedToken &tokenSpan)
{
    const QString trimmedName = newName.trimmed();
    const bool requiresQuoting = trimmedName.isEmpty()
        || trimmedName.contains(QRegularExpression(QStringLiteral(R"([\s#%\\'\"])")));

    if (tokenSpan.type != TherionTokenType::QuotedString && !requiresQuoting) {
        return trimmedName;
    }

    const QChar quoteCharacter = (tokenSpan.start < lineText.size() && lineText.at(tokenSpan.start) == QLatin1Char('\''))
        ? QLatin1Char('\'')
        : QLatin1Char('"');
    return quoteToken(trimmedName, quoteCharacter);
}

QString sanitizeScrapName(const QString &preferredName)
{
    QString source = preferredName.trimmed().toLower();
    if (source.isEmpty()) {
        source = QStringLiteral("new-scrap");
    }

    QString normalized;
    normalized.reserve(source.size());
    bool previousDash = false;
    for (const QChar character : source) {
        const bool allowed = character.isLetterOrNumber()
            || character == QLatin1Char('-')
            || character == QLatin1Char('_')
            || character == QLatin1Char('.');

        if (allowed) {
            normalized.append(character);
            previousDash = false;
            continue;
        }

        if (!previousDash) {
            normalized.append(QLatin1Char('-'));
            previousDash = true;
        }
    }

    while (normalized.startsWith(QLatin1Char('-'))) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(QLatin1Char('-'))) {
        normalized.chop(1);
    }

    if (normalized.isEmpty()) {
        return QStringLiteral("new-scrap");
    }

    return normalized;
}

int lineCountForText(const QString &text)
{
    if (text.isEmpty()) {
        return 0;
    }

    int lineCount = text.count(QLatin1Char('\n'));
    if (!text.endsWith(QLatin1Char('\n'))) {
        ++lineCount;
    }

    return qMax(1, lineCount);
}

QString formatCoordinate(qreal value)
{
    return QString::number(value, 'f', 1);
}

QPair<int, int> coordinateTokenPair(const TherionParsedLine &parsedLine)
{
    int firstIndex = -1;
    int secondIndex = -1;
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        if (!tokenLooksNumeric(parsedLine.tokens.at(index))) {
            continue;
        }

        if (firstIndex < 0) {
            firstIndex = index;
            continue;
        }

        secondIndex = index;
        break;
    }

    return qMakePair(firstIndex, secondIndex);
}

QVector<QPair<int, int>> coordinateTokenPairsForLine(const TherionParsedLine &parsedLine, int startTokenIndex)
{
    QVector<QPair<int, int>> pairs;
    QVector<int> numericIndices;
    numericIndices.reserve(parsedLine.tokens.size());

    const int firstTokenIndex = qMax(0, startTokenIndex);
    for (int index = firstTokenIndex; index < parsedLine.tokens.size(); ++index) {
        if (!tokenLooksNumeric(parsedLine.tokens.at(index))) {
            continue;
        }

        numericIndices.append(index);
    }

    for (int index = 0; index + 1 < numericIndices.size(); index += 2) {
        pairs.append(qMakePair(numericIndices.at(index), numericIndices.at(index + 1)));
    }

    return pairs;
}

QString formatVerticesInline(const QVector<QPointF> &vertices)
{
    QStringList tokens;
    tokens.reserve(vertices.size() * 2);
    for (const QPointF &vertex : vertices) {
        tokens.append(formatCoordinate(vertex.x()));
        tokens.append(formatCoordinate(vertex.y()));
    }

    return tokens.join(QLatin1Char(' '));
}

int lastEndscrapLineIndex(const QStringList &lines)
{
    int foundIndex = -1;
    for (int index = 0; index < lines.size(); ++index) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(index), index + 1);
        if (parsedLine.directive == QStringLiteral("endscrap")) {
            foundIndex = index;
        }
    }

    return foundIndex;
}

QStringList splitLinesNormalized(const QString &contents)
{
    QStringList lines = contents.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    return lines;
}
}

bool TherionDocumentEditor::rewriteStructureEntryName(QString *contents,
                                                      int lineNumber,
                                                      const QString &category,
                                                      const QString &newName,
                                                      QString *errorMessage)
{
    if (contents == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No document contents are available.");
        }
        return false;
    }

    const QString trimmedName = newName.trimmed();
    if (trimmedName.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The new structure name is empty.");
        }
        return false;
    }

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line number is invalid.");
        }
        return false;
    }

    const QString lineEnding = contents->contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = contents->split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }
    if (lineNumber > lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line no longer exists.");
        }
        return false;
    }

    QString lineText = lines.at(lineNumber - 1);
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lineText, lineNumber);
    if (parsedLine.tokens.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line does not contain a structure object.");
        }
        return false;
    }

    const int tokenIndex = nameTokenIndexForCategory(category, parsedLine);
    if (tokenIndex < 0 || tokenIndex >= parsedLine.tokenSpans.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected structure item cannot be rewritten.");
        }
        return false;
    }

    const TherionParsedToken tokenSpan = parsedLine.tokenSpans.at(tokenIndex);
    if (tokenSpan.start < 0 || tokenSpan.length < 0 || tokenSpan.start + tokenSpan.length > lineText.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line could not be rewritten.");
        }
        return false;
    }

    const QString replacementToken = replacementTokenForLine(trimmedName, lineText, tokenSpan);
    lineText.replace(tokenSpan.start, tokenSpan.length, replacementToken);
    lines[lineNumber - 1] = lineText;
    *contents = lines.join(lineEnding);
    return true;
}

bool TherionDocumentEditor::appendScrapBlock(QString *contents,
                                             const QString &preferredName,
                                             int *insertedLineNumber,
                                             QString *errorMessage)
{
    if (contents == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No document contents are available.");
        }
        return false;
    }

    const QString lineEnding = contents->contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(*contents);

    QSet<QString> existingNames;
    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive != QStringLiteral("scrap")) {
            continue;
        }
        if (parsedLine.tokens.size() < 2) {
            continue;
        }

        existingNames.insert(parsedLine.tokens.at(1).trimmed().toLower());
    }

    const QString baseName = sanitizeScrapName(preferredName);
    QString resolvedName = baseName;
    int suffix = 2;
    while (existingNames.contains(resolvedName.toLower())) {
        resolvedName = QStringLiteral("%1-%2").arg(baseName).arg(suffix++);
    }

    QString updated = *contents;
    if (!updated.isEmpty() && !updated.endsWith(QLatin1Char('\n'))) {
        updated += lineEnding;
    }
    if (!updated.isEmpty()) {
        updated += lineEnding;
    }

    const int scrapLineNumber = lineCountForText(updated) + 1;
    updated += QStringLiteral("scrap %1").arg(resolvedName);
    updated += lineEnding;
    updated += QStringLiteral("endscrap");
    updated += lineEnding;

    *contents = updated;
    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = scrapLineNumber;
    }

    return true;
}

bool TherionDocumentEditor::appendDraftGeometry(QString *contents,
                                                const QString &kind,
                                                const QVector<QPointF> &vertices,
                                                int *insertedLineNumber,
                                                QString *errorMessage)
{
    if (contents == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No document contents are available.");
        }
        return false;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind != QStringLiteral("point")
        && normalizedKind != QStringLiteral("line")
        && normalizedKind != QStringLiteral("area")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Unsupported draft geometry kind.");
        }
        return false;
    }

    const int minimumVertices = normalizedKind == QStringLiteral("point")
        ? 1
        : (normalizedKind == QStringLiteral("line") ? 2 : 3);
    if (vertices.size() < minimumVertices) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("Draft geometry does not contain enough vertices.");
        }
        return false;
    }

    QString updated = *contents;
    const QString lineEnding = updated.contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = splitLinesNormalized(updated);
    int insertionIndex = lastEndscrapLineIndex(lines);

    if (insertionIndex < 0) {
        if (!appendScrapBlock(&updated, QStringLiteral("map-draft"), nullptr, errorMessage)) {
            return false;
        }

        lines = splitLinesNormalized(updated);
        insertionIndex = lastEndscrapLineIndex(lines);
        if (insertionIndex < 0) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Unable to resolve a scrap insertion target.");
            }
            return false;
        }
    }

    QStringList geometryLines;
    if (normalizedKind == QStringLiteral("point")) {
        const QPointF anchor = vertices.first();
        geometryLines.append(QStringLiteral("  point %1 %2 station -name draft-point")
                                 .arg(formatCoordinate(anchor.x()), formatCoordinate(anchor.y())));
    } else if (normalizedKind == QStringLiteral("line")) {
        geometryLines.append(QStringLiteral("  line wall"));
        geometryLines.append(QStringLiteral("    %1").arg(formatVerticesInline(vertices.mid(0, 2))));
        geometryLines.append(QStringLiteral("  endline"));
    } else {
        geometryLines.append(QStringLiteral("  area water"));
        geometryLines.append(QStringLiteral("    %1").arg(formatVerticesInline(vertices.mid(0, 3))));
        geometryLines.append(QStringLiteral("  endarea"));
    }

    int nextInsertionIndex = insertionIndex;
    for (const QString &line : geometryLines) {
        lines.insert(nextInsertionIndex, line);
        ++nextInsertionIndex;
    }

    *contents = lines.join(lineEnding);
    if (!contents->endsWith(QLatin1Char('\n'))) {
        *contents += lineEnding;
    }

    if (insertedLineNumber != nullptr) {
        *insertedLineNumber = insertionIndex + 1;
    }

    return true;
}

bool TherionDocumentEditor::rewritePointCoordinates(QString *contents,
                                                    int lineNumber,
                                                    const QPointF &point,
                                                    QString *errorMessage)
{
    if (contents == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No document contents are available.");
        }
        return false;
    }

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line number is invalid.");
        }
        return false;
    }

    const QString lineEnding = contents->contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = contents->split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
    }

    if (lineNumber > lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line no longer exists.");
        }
        return false;
    }

    QString lineText = lines.at(lineNumber - 1);
    const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lineText, lineNumber);
    if (parsedLine.directive != QStringLiteral("point") && parsedLine.directive != QStringLiteral("station")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line is not a writable point geometry.");
        }
        return false;
    }

    const QPair<int, int> tokenPair = coordinateTokenPair(parsedLine);
    if (tokenPair.first < 0 || tokenPair.second < 0
        || tokenPair.first >= parsedLine.tokenSpans.size()
        || tokenPair.second >= parsedLine.tokenSpans.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected point line does not contain writable coordinates.");
        }
        return false;
    }

    const TherionParsedToken secondToken = parsedLine.tokenSpans.at(tokenPair.second);
    const TherionParsedToken firstToken = parsedLine.tokenSpans.at(tokenPair.first);
    if (firstToken.start < 0 || secondToken.start < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected point coordinates could not be rewritten.");
        }
        return false;
    }

    lineText.replace(secondToken.start, secondToken.length, formatCoordinate(point.y()));
    lineText.replace(firstToken.start, firstToken.length, formatCoordinate(point.x()));
    lines[lineNumber - 1] = lineText;
    *contents = lines.join(lineEnding);
    return true;
}

bool TherionDocumentEditor::rewriteLineAreaVertex(QString *contents,
                                                  int lineNumber,
                                                  const QString &kind,
                                                  int vertexIndex,
                                                  const QPointF &point,
                                                  QString *errorMessage)
{
    if (contents == nullptr) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("No document contents are available.");
        }
        return false;
    }

    if (lineNumber <= 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line number is invalid.");
        }
        return false;
    }

    if (vertexIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected vertex index is invalid.");
        }
        return false;
    }

    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind != QStringLiteral("line") && normalizedKind != QStringLiteral("area")) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected geometry kind is not writable.");
        }
        return false;
    }

    const QString lineEnding = contents->contains(QStringLiteral("\r\n")) ? QStringLiteral("\r\n") : QStringLiteral("\n");
    QStringList lines = splitLinesNormalized(*contents);
    if (lineNumber > lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line no longer exists.");
        }
        return false;
    }

    const int blockStartLineIndex = lineNumber - 1;
    const TherionParsedLine startLine = TherionDocumentParser::parseLine(lines.at(blockStartLineIndex), lineNumber);
    if (startLine.directive != normalizedKind) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected line is not a writable %1 geometry block.").arg(normalizedKind);
        }
        return false;
    }

    const QString blockEndDirective = normalizedKind == QStringLiteral("line")
        ? QStringLiteral("endline")
        : QStringLiteral("endarea");
    int blockEndLineIndex = -1;
    for (int candidateIndex = blockStartLineIndex + 1; candidateIndex < lines.size(); ++candidateIndex) {
        const TherionParsedLine candidateLine = TherionDocumentParser::parseLine(lines.at(candidateIndex), candidateIndex + 1);
        if (candidateLine.directive == blockEndDirective) {
            blockEndLineIndex = candidateIndex;
            break;
        }
    }

    if (blockEndLineIndex < 0) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected %1 block is missing %2.").arg(normalizedKind, blockEndDirective);
        }
        return false;
    }

    struct CoordinateTokenReference
    {
        int lineIndex = -1;
        TherionParsedToken xToken;
        TherionParsedToken yToken;
    };

    QVector<CoordinateTokenReference> references;
    for (int lineIndex = blockStartLineIndex; lineIndex < blockEndLineIndex; ++lineIndex) {
        const TherionParsedLine parsedLine = TherionDocumentParser::parseLine(lines.at(lineIndex), lineIndex + 1);
        const int startTokenIndex = lineIndex == blockStartLineIndex ? 1 : 0;
        const QVector<QPair<int, int>> pairs = coordinateTokenPairsForLine(parsedLine, startTokenIndex);
        for (const QPair<int, int> &pair : pairs) {
            if (pair.first < 0 || pair.second < 0
                || pair.first >= parsedLine.tokenSpans.size()
                || pair.second >= parsedLine.tokenSpans.size()) {
                continue;
            }

            CoordinateTokenReference reference;
            reference.lineIndex = lineIndex;
            reference.xToken = parsedLine.tokenSpans.at(pair.first);
            reference.yToken = parsedLine.tokenSpans.at(pair.second);
            references.append(reference);
        }
    }

    if (vertexIndex >= references.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected %1 block does not contain a writable vertex at index %2.")
                                .arg(normalizedKind)
                                .arg(vertexIndex);
        }
        return false;
    }

    const CoordinateTokenReference reference = references.at(vertexIndex);
    if (reference.lineIndex < 0 || reference.lineIndex >= lines.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected %1 vertex could not be rewritten.").arg(normalizedKind);
        }
        return false;
    }

    QString lineText = lines.at(reference.lineIndex);
    if (reference.xToken.start < 0 || reference.yToken.start < 0
        || reference.xToken.length < 0 || reference.yToken.length < 0
        || reference.xToken.start + reference.xToken.length > lineText.size()
        || reference.yToken.start + reference.yToken.length > lineText.size()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected %1 vertex coordinates could not be rewritten.").arg(normalizedKind);
        }
        return false;
    }

    lineText.replace(reference.yToken.start, reference.yToken.length, formatCoordinate(point.y()));
    lineText.replace(reference.xToken.start, reference.xToken.length, formatCoordinate(point.x()));
    lines[reference.lineIndex] = lineText;
    *contents = lines.join(lineEnding);
    return true;
}
}
