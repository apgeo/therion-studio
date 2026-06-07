#include "TherionBackgroundMetadata.h"

#include "TherionStringUtils.h"

#include <QDir>
#include <QFileInfo>

namespace TherionStudio
{
namespace
{
void skipWhitespace(const QString &text, int *position)
{
    if (position == nullptr) {
        return;
    }

    while (*position < text.size() && text.at(*position).isSpace()) {
        ++(*position);
    }
}

bool readBracedGroup(const QString &text, int *position, QString *groupText)
{
    if (position == nullptr || groupText == nullptr) {
        return false;
    }

    skipWhitespace(text, position);
    if (*position >= text.size() || text.at(*position) != QLatin1Char('{')) {
        return false;
    }

    ++(*position);
    const int contentStart = *position;
    int depth = 1;
    while (*position < text.size() && depth > 0) {
        const QChar character = text.at(*position);
        if (character == QLatin1Char('{')) {
            ++depth;
        } else if (character == QLatin1Char('}')) {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        ++(*position);
    }

    if (*position >= text.size() || text.at(*position) != QLatin1Char('}')) {
        return false;
    }

    const int contentEnd = *position;
    ++(*position);
    *groupText = text.mid(contentStart, contentEnd - contentStart).trimmed();
    return true;
}

QString readSimpleToken(const QString &text, int *position)
{
    if (position == nullptr) {
        return QString();
    }

    skipWhitespace(text, position);
    if (*position >= text.size()) {
        return QString();
    }

    const QChar first = text.at(*position);
    if (first == QLatin1Char('{')) {
        QString groupText;
        if (readBracedGroup(text, position, &groupText)) {
            return groupText;
        }
        return QString();
    }

    if (first == QLatin1Char('"') || first == QLatin1Char('\'')) {
        const QChar quote = first;
        ++(*position);
        const int start = *position;
        while (*position < text.size() && text.at(*position) != quote) {
            ++(*position);
        }
        const int end = *position;
        if (*position < text.size() && text.at(*position) == quote) {
            ++(*position);
        }
        return text.mid(start, end - start).trimmed();
    }

    const int start = *position;
    while (*position < text.size() && !text.at(*position).isSpace()) {
        ++(*position);
    }
    return text.mid(start, *position - start).trimmed();
}

bool tryParseLeadingNumber(QString token, qreal *value)
{
    if (value == nullptr) {
        return false;
    }

    token = token.trimmed();
    while (!token.isEmpty()) {
        const QChar character = token.front();
        if (character.isDigit() || character == QLatin1Char('+') || character == QLatin1Char('-') || character == QLatin1Char('.')) {
            break;
        }
        token.remove(0, 1);
    }

    while (!token.isEmpty()) {
        const QChar character = token.back();
        if (character.isDigit() || character == QLatin1Char('.')) {
            break;
        }
        token.chop(1);
    }

    if (token.isEmpty()) {
        return false;
    }

    bool ok = false;
    const qreal parsed = token.toDouble(&ok);
    if (!ok) {
        return false;
    }

    *value = parsed;
    return true;
}

QString normalizeStationToken(const QString &token)
{
    QString normalized = token.trimmed();
    while (normalized.startsWith(QLatin1Char('{')) || normalized.startsWith(QLatin1Char('}'))) {
        normalized.remove(0, 1);
    }
    while (normalized.endsWith(QLatin1Char('}')) || normalized.endsWith(QLatin1Char('{'))) {
        normalized.chop(1);
    }
    return normalized.trimmed();
}
}

QVector<TherionBackgroundReference> parseTherionBackgroundReferences(const QString &documentText,
                                                                     const QString &documentPath)
{
    QVector<TherionBackgroundReference> references;
    const QString baseDirectory = QFileInfo(documentPath).absolutePath();
    const QStringList lines = splitLinesNormalizingLineEndings(documentText);
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString trimmed = lines.at(lineIndex).trimmed();
        if (!trimmed.contains(QStringLiteral("##XTHERION##")) || !trimmed.contains(QStringLiteral("xth_me_image_insert"))) {
            continue;
        }

        int keywordIndex = trimmed.indexOf(QStringLiteral("xth_me_image_insert"));
        if (keywordIndex < 0) {
            continue;
        }

        int position = keywordIndex + QStringLiteral("xth_me_image_insert").size();
        QString firstGroup;
        QString secondGroup;
        if (!readBracedGroup(trimmed, &position, &firstGroup)) {
            continue;
        }
        if (!readBracedGroup(trimmed, &position, &secondGroup)) {
            secondGroup = readSimpleToken(trimmed, &position);
        }
        if (secondGroup.isEmpty()) {
            continue;
        }

        const QString imageToken = readSimpleToken(trimmed, &position);
        if (imageToken.isEmpty()) {
            continue;
        }

        QString absolutePath = imageToken;
        if (QDir::isRelativePath(absolutePath) && !baseDirectory.isEmpty()) {
            absolutePath = QDir(baseDirectory).absoluteFilePath(absolutePath);
        }
        absolutePath = QFileInfo(absolutePath).absoluteFilePath();
        if (absolutePath.isEmpty()) {
            continue;
        }

        TherionBackgroundReference reference{};
        reference.absolutePath = absolutePath;
        reference.xviReference = reference.absolutePath.endsWith(QStringLiteral(".xvi"), Qt::CaseInsensitive);
        reference.metadataTopEdgeAnchor = !reference.xviReference;
        reference.lineNumber = lineIndex + 1;

        const QStringList firstParts = tokenizeWhitespace(firstGroup);
        const QStringList secondParts = tokenizeWhitespace(secondGroup);
        if (!firstParts.isEmpty() && !secondParts.isEmpty()) {
            qreal baseX = 0.0;
            qreal baseY = 0.0;
            if (tryParseLeadingNumber(firstParts.first(), &baseX)
                && tryParseLeadingNumber(secondParts.first(), &baseY)) {
                reference.basePosition = QPointF(baseX, baseY);
                reference.hasBasePosition = true;
            }
        }

        if (firstParts.size() >= 2) {
            qreal visibility = 0.0;
            if (tryParseLeadingNumber(firstParts.at(1), &visibility)) {
                reference.visible = visibility > 0.0;
                reference.hasVisibility = true;
            }
        }

        if (firstParts.size() >= 3) {
            qreal imageScale = 0.0;
            if (tryParseLeadingNumber(firstParts.at(2), &imageScale) && imageScale > 0.0) {
                reference.imageScale = imageScale;
                reference.hasImageScale = true;
            }
        }

        if (secondParts.size() >= 2) {
            const QString candidateRoot = normalizeStationToken(secondParts.at(1));
            if (!candidateRoot.isEmpty() && candidateRoot != QStringLiteral("0")) {
                reference.rootStationName = candidateRoot;
            }
        }
        references.append(reference);
    }

    return references;
}

TherionAreaAdjust parseTherionAreaAdjust(const QString &documentText)
{
    const QStringList lines = splitLinesNormalizingLineEndings(documentText);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.contains(QStringLiteral("##XTHERION##")) || !trimmed.contains(QStringLiteral("xth_me_area_adjust"))) {
            continue;
        }

        const int keywordIndex = trimmed.indexOf(QStringLiteral("xth_me_area_adjust"));
        if (keywordIndex < 0) {
            continue;
        }

        const QString payload = trimmed.mid(keywordIndex + QStringLiteral("xth_me_area_adjust").size());
        const QStringList tokens = tokenizeWhitespace(payload);
        QVector<qreal> numbers;
        numbers.reserve(tokens.size());
        for (const QString &token : tokens) {
            qreal value = 0.0;
            if (tryParseLeadingNumber(token, &value)) {
                numbers.append(value);
            }
        }

        if (numbers.size() < 4) {
            continue;
        }

        const qreal left = qMin(numbers.at(0), numbers.at(2));
        const qreal right = qMax(numbers.at(0), numbers.at(2));
        const qreal bottom = qMin(numbers.at(1), numbers.at(3));
        const qreal top = qMax(numbers.at(1), numbers.at(3));
        const QRectF rect(QPointF(left, bottom), QPointF(right, top));
        if (rect.isValid() && rect.width() > 0.0 && rect.height() > 0.0) {
            return TherionAreaAdjust{rect, true};
        }
    }

    return TherionAreaAdjust{};
}
}
