#pragma once

#include <QString>
#include <QStringList>

class QJsonObject;

namespace TherionStudio
{
struct TherionParsedLine;

namespace BlockEditorDirectiveRules
{
QString blockDisplayName(const TherionParsedLine &parsedLine);
QString blockDisplayNameForKind(const QString &kind, const TherionParsedLine &parsedLine);
QString blockDisplayKindLabel(const QString &kind);
QString mapObjectReferenceKind();
bool isMapObjectReferenceKind(const QString &kind);
bool isMapObjectReferenceCandidateLine(const QString &activeScope,
                                       const TherionParsedLine &parsedLine,
                                       bool commandDirective);
void resetCatalogBlockDirectiveMetadataToDefaults();
void applyCatalogBlockDirectiveMetadata(const QJsonObject &catalogObject);
bool isBlockOpeningDirective(const QString &directive);
bool isContainerBlockDirective(const QString &directive);
bool isContainerDirectiveInstance(const QString &directive, const TherionParsedLine &parsedLine);
bool isBlockClosingDirective(const QString &directive);
QString closingDirectiveFor(const QString &openingDirective);
QString normalizeDirective(const QString &directive);
bool isEncodingDirective(const QString &directive);
bool isFullLineComment(const TherionParsedLine &parsedLine);
QString normalizedCompletionContextToken(const QString &token);
QString contextDisplayLabel(const QString &contextToken);
QString completionOpeningDirectiveForClosing(const QString &directive);
QString completionClosingDirectiveForOpening(const QString &directive);
int findMatchingBlockEndLine(const QStringList &lines,
                             int openingLineNumber,
                             const QString &openingDirective,
                             const QString &closingDirective);
}
}
