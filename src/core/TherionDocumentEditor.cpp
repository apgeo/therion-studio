#include "TherionDocumentEditor.h"

#include "TherionDocumentParser.h"

#include <QFileInfo>
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
}