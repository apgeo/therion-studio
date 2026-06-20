#include "BlockEditorDirectiveRules.h"

#include "../../../core/TherionSourceLogicalDocument.h"
#include "../../../core/TherionSourceSnapshotCache.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>

namespace
{
QString normalizeDirectiveToken(const QString &directive)
{
    const QString normalized = directive.trimmed().toLower();
    if (normalized == QStringLiteral("centreline")) {
        return QStringLiteral("centerline");
    }
    if (normalized == QStringLiteral("endcentreline")) {
        return QStringLiteral("endcenterline");
    }
    return normalized;
}

QHash<QString, QString> defaultBlockOpenToCloseMap()
{
    return {
        {QStringLiteral("survey"), QStringLiteral("endsurvey")},
        {QStringLiteral("centerline"), QStringLiteral("endcenterline")},
        {QStringLiteral("map"), QStringLiteral("endmap")},
        {QStringLiteral("scrap"), QStringLiteral("endscrap")},
        {QStringLiteral("surface"), QStringLiteral("endsurface")},
        {QStringLiteral("layout"), QStringLiteral("endlayout")},
        {QStringLiteral("lookup"), QStringLiteral("endlookup")},
        {QStringLiteral("line"), QStringLiteral("endline")},
        {QStringLiteral("area"), QStringLiteral("endarea")},
    };
}

QHash<QString, QString> invertBlockOpenCloseMap(const QHash<QString, QString> &openToCloseMap)
{
    QHash<QString, QString> closeToOpenMap;
    for (auto iterator = openToCloseMap.constBegin(); iterator != openToCloseMap.constEnd(); ++iterator) {
        const QString openDirective = normalizeDirectiveToken(iterator.key());
        const QString closeDirective = normalizeDirectiveToken(iterator.value());
        if (openDirective.isEmpty() || closeDirective.isEmpty()) {
            continue;
        }
        closeToOpenMap.insert(closeDirective, openDirective);
    }
    return closeToOpenMap;
}

QHash<QString, QString> gBlockOpenToCloseMap = defaultBlockOpenToCloseMap();
QHash<QString, QString> gBlockCloseToOpenMap = invertBlockOpenCloseMap(gBlockOpenToCloseMap);
}

namespace TherionStudio::BlockEditorDirectiveRules
{
QString mapObjectReferenceKind()
{
    return QStringLiteral("__map_object_reference");
}

bool isMapObjectReferenceKind(const QString &kind)
{
    return normalizeDirectiveToken(kind) == mapObjectReferenceKind();
}

QString unrecognizedKind()
{
    return QStringLiteral("__unrecognized");
}

bool isUnrecognizedKind(const QString &kind)
{
    return normalizeDirectiveToken(kind) == unrecognizedKind();
}

QString blockDisplayName(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() >= 2) {
        return parsedLine.tokens.at(1);
    }
    return QString();
}

QString blockDisplayNameForKind(const QString &kind, const TherionParsedLine &parsedLine)
{
    if (isMapObjectReferenceKind(kind)) {
        return parsedLine.tokens.isEmpty() ? QString() : parsedLine.tokens.first();
    }
    if (isUnrecognizedKind(kind)) {
        return parsedLine.rawText.trimmed();
    }
    return blockDisplayName(parsedLine);
}

QString blockDisplayKindLabel(const QString &kind)
{
    if (isMapObjectReferenceKind(kind)) {
        return QObject::tr("Object Reference");
    }
    if (isUnrecognizedKind(kind)) {
        return QObject::tr("Unrecognized");
    }
    return kind;
}

bool isMapObjectReferenceCandidateLine(const QString &activeScope,
                                       const TherionParsedLine &parsedLine,
                                       bool commandDirective)
{
    if (normalizeDirectiveToken(activeScope) != QStringLiteral("map") || commandDirective) {
        return false;
    }
    const QString directive = normalizeDirectiveToken(parsedLine.directive);
    if (directive.isEmpty()
        || isBlockOpeningDirective(directive)
        || isBlockClosingDirective(directive)) {
        return false;
    }
    return !parsedLine.tokens.isEmpty();
}

void resetCatalogBlockDirectiveMetadataToDefaults()
{
    gBlockOpenToCloseMap = defaultBlockOpenToCloseMap();
    gBlockCloseToOpenMap = invertBlockOpenCloseMap(gBlockOpenToCloseMap);
}

void applyCatalogBlockDirectiveMetadata(const QJsonObject &catalogObject)
{
    const QJsonArray blockPairs = catalogObject.value(QStringLiteral("block_pairs")).toArray();
    if (blockPairs.isEmpty()) {
        return;
    }

    QHash<QString, QString> openToCloseMap;
    for (const QJsonValue &pairValue : blockPairs) {
        const QJsonObject pairObject = pairValue.toObject();
        const QString openDirective = normalizeDirectiveToken(pairObject.value(QStringLiteral("open_directive")).toString());
        const QString closeDirective = normalizeDirectiveToken(pairObject.value(QStringLiteral("close_directive")).toString());
        if (openDirective.isEmpty() || closeDirective.isEmpty() || openDirective.startsWith(QStringLiteral("end"))) {
            continue;
        }
        openToCloseMap.insert(openDirective, closeDirective);
    }

    if (openToCloseMap.isEmpty()) {
        return;
    }

    gBlockOpenToCloseMap = openToCloseMap;
    gBlockCloseToOpenMap = invertBlockOpenCloseMap(gBlockOpenToCloseMap);
}

bool isBlockOpeningDirective(const QString &directive)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    return normalizedDirective == QStringLiteral("data")
        || gBlockOpenToCloseMap.contains(normalizedDirective);
}

bool isContainerBlockDirective(const QString &directive)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    return gBlockOpenToCloseMap.contains(normalizedDirective);
}

bool isContainerDirectiveInstance(const QString &directive, const TherionParsedLine &parsedLine)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    if (!gBlockOpenToCloseMap.contains(normalizedDirective)) {
        return false;
    }

    if (normalizedDirective == QStringLiteral("source") && parsedLine.tokens.size() > 1) {
        return false;
    }

    return true;
}

bool isBlockClosingDirective(const QString &directive)
{
    const QString normalizedDirective = normalizeDirectiveToken(directive);
    return gBlockCloseToOpenMap.contains(normalizedDirective);
}

QString closingDirectiveFor(const QString &openingDirective)
{
    return gBlockOpenToCloseMap.value(normalizeDirectiveToken(openingDirective));
}

QString normalizeDirective(const QString &directive)
{
    return normalizeDirectiveToken(directive);
}

bool isEncodingDirective(const QString &directive)
{
    return normalizeDirective(directive) == QStringLiteral("encoding");
}

bool isFullLineComment(const TherionParsedLine &parsedLine)
{
    if (parsedLine.commentStart < 0) {
        return false;
    }
    const QString beforeComment = parsedLine.rawText.left(parsedLine.commentStart).trimmed();
    return beforeComment.isEmpty() && !parsedLine.commentText.trimmed().isEmpty();
}

QString normalizedCompletionContextToken(const QString &token)
{
    const QString normalized = normalizeDirective(token.trimmed());
    if (normalized == QStringLiteral("all")) {
        return QStringLiteral("all");
    }
    if (normalized == QStringLiteral("none")) {
        return QStringLiteral("none");
    }
    static const QRegularExpression contextTokenPattern(QStringLiteral("^[a-z][a-z0-9-]*$"));
    if (contextTokenPattern.match(normalized).hasMatch()) {
        return normalized;
    }
    return QString();
}

QString contextDisplayLabel(const QString &contextToken)
{
    const QString normalized = normalizedCompletionContextToken(contextToken);
    if (normalized == QStringLiteral("none")) {
        return QObject::tr("Top-level");
    }
    if (normalized == QStringLiteral("all")) {
        return QObject::tr("All");
    }

    QStringList parts = normalized.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    for (QString &part : parts) {
        if (!part.isEmpty()) {
            part[0] = part.at(0).toUpper();
        }
    }
    return QObject::tr("Inside %1").arg(parts.join(QLatin1Char(' ')));
}

QString completionOpeningDirectiveForClosing(const QString &directive)
{
    return gBlockCloseToOpenMap.value(normalizeDirectiveToken(directive));
}

QString completionClosingDirectiveForOpening(const QString &directive)
{
    return gBlockOpenToCloseMap.value(normalizeDirectiveToken(directive));
}

int findMatchingBlockEndLine(const QStringList &lines,
                             int openingLineNumber,
                             const QString &openingDirective,
                             const QString &closingDirective)
{
    if (openingLineNumber <= 0 || openingLineNumber > lines.size()) {
        return -1;
    }

    TherionSourceSnapshotCache sourceSnapshotCache;
    TherionSourceDocumentMetadata metadata;
    metadata.revisionId = 1;
    const TherionSourceLogicalDocument &logicalDocument =
        sourceSnapshotCache.logicalDocument(lines.join(QLatin1Char('\n')), metadata);

    int openingCommandIndex = -1;
    for (int index = 0; index < logicalDocument.commands().size(); ++index) {
        const TherionSourceLogicalCommand &command = logicalDocument.commands().at(index);
        if (openingLineNumber < command.startLineNumber || openingLineNumber > command.endLineNumber) {
            continue;
        }
        openingCommandIndex = index;
        break;
    }
    if (openingCommandIndex < 0) {
        return -1;
    }

    int depth = 0;
    for (int index = openingCommandIndex; index < logicalDocument.commands().size(); ++index) {
        const TherionSourceLogicalCommand &command = logicalDocument.commands().at(index);
        const QString directive = normalizeDirective(command.parsed.directive);
        if (directive == openingDirective) {
            ++depth;
            continue;
        }
        if (directive != closingDirective) {
            continue;
        }

        --depth;
        if (depth == 0) {
            return command.endLineNumber;
        }
    }

    return -1;
}
}
