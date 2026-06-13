#include "ProjectStructureIndex.h"

#include "DocumentFile.h"
#include "TherionCommandLineModel.h"
#include "TherionFileTypes.h"
#include "TherionSourceLogicalDocument.h"
#include "TherionSourceReferenceResolver.h"
#include "TherionTokenRules.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>

namespace TherionStudio
{
namespace
{
struct ProjectBlock
{
    ProjectStructureEntryKind kind = ProjectStructureEntryKind::Unknown;
    QString name;
    QString objectId;
    QString namespacePath;
    bool createsNamespace = true;
};

struct ParsedFileCache
{
    QHash<QString, TherionSourceLogicalDocument> logicalDocuments;
};

struct ProjectObjectIdentityGenerator
{
    QHash<QString, int> siblingOrdinalsByScope;

    QString nextObjectId(const QString &category,
                         const QString &name,
                         const QString &sourceFile,
                         const QString &parentObjectId);
};

struct MapReferenceScanResult
{
    QHash<QString, QSet<QString>> scrapReferencesByMapKey;
    QHash<QString, QSet<QString>> childMapReferencesByMapKey;
    QHash<QString, QSet<QString>> previewReferencesByMapKey;
    QVector<ProjectIndexDiagnostic> diagnostics;
};

struct RootConfigResolution
{
    QVector<QString> rootFiles;
    QString configPath;
    QString errorMessage;
};

QString normalizedFilePathKey(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    QFileInfo fileInfo(path);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    const QString resolvedPath = canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
    return QFileInfo(resolvedPath).absoluteFilePath();
}

bool pathIsInsideDirectory(const QString &path, const QString &directoryPath)
{
    if (path.isEmpty() || directoryPath.isEmpty()) {
        return false;
    }

    const QString relativePath = QDir(directoryPath).relativeFilePath(path);
    return relativePath != QStringLiteral("..")
        && !relativePath.startsWith(QStringLiteral("../"))
        && !relativePath.startsWith(QStringLiteral("..\\"))
        && !QDir::isAbsolutePath(relativePath);
}

TherionSourceLogicalDocument logicalDocumentForText(const QString &text)
{
    return TherionSourceLogicalDocument::fromText(text);
}

QString sectionNameFromLine(const TherionParsedLine &parsedLine);
ProjectStructureEntryKind objectKindFromLine(const TherionParsedLine &parsedLine);
QString objectNameFromLine(const TherionParsedLine &parsedLine);
QString normalizedStructureDirective(const QString &directive);

QString normalizedIdentityToken(const QString &value)
{
    QString normalized = value.trimmed().toLower();
    normalized.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QStringLiteral(" "));
    return normalized;
}

QString categoryIdentityToken(const QString &category)
{
    QString normalized = normalizedIdentityToken(category);
    normalized.replace(QLatin1Char(' '), QLatin1Char('-'));
    return normalized;
}

QString objectNameIdentityToken(const QString &name)
{
    const QString normalized = normalizedIdentityToken(name);
    return normalized.isEmpty() ? QStringLiteral("anonymous") : normalized;
}

QString mapReferenceLookupKey(QString referenceName)
{
    referenceName = referenceName.trimmed();
    const int namespaceIndex = referenceName.indexOf(QLatin1Char('@'));
    if (namespaceIndex >= 0) {
        referenceName = referenceName.left(namespaceIndex);
    }

    return referenceName.trimmed().toLower();
}

QString normalizedNamespaceToken(QString namespacePath)
{
    namespacePath = namespacePath.trimmed().toLower();
    namespacePath.replace(QRegularExpression(QStringLiteral(R"(\s+)")), QString());
    return namespacePath;
}

QString namespacePathWithChild(const QString &currentNamespacePath, const QString &childName)
{
    const QString child = childName.trimmed();
    if (child.isEmpty()) {
        return currentNamespacePath;
    }
    if (currentNamespacePath.trimmed().isEmpty()) {
        return child;
    }

    return QStringLiteral("%1.%2").arg(child, currentNamespacePath);
}

struct MapReferenceParts
{
    QString name;
    QString namespacePath;
    bool hasNamespace = false;
};

MapReferenceParts parseMapReference(const QString &referenceName)
{
    MapReferenceParts parts;
    const QString trimmedReference = referenceName.trimmed();
    const int namespaceIndex = trimmedReference.indexOf(QLatin1Char('@'));
    if (namespaceIndex < 0) {
        parts.name = trimmedReference;
        return parts;
    }

    parts.name = trimmedReference.left(namespaceIndex).trimmed();
    parts.namespacePath = trimmedReference.mid(namespaceIndex + 1).trimmed();
    parts.hasNamespace = true;
    return parts;
}

QString namespacedReferenceLookupKey(const QString &name, const QString &namespacePath)
{
    return QStringLiteral("%1@%2").arg(mapReferenceLookupKey(name), normalizedNamespaceToken(namespacePath));
}

struct ReferenceKeyIndex
{
    QHash<QString, QVector<QString>> keysByNameAndNamespace;
};

enum class ReferenceResolutionState
{
    Missing,
    Unique,
    Ambiguous,
};

enum class MapCompositionContentKind
{
    Unknown,
    Map,
    Scrap,
};

struct ReferenceResolution
{
    ReferenceResolutionState state = ReferenceResolutionState::Missing;
    QString key;
    int candidateCount = 0;
};

ReferenceKeyIndex entryKeysByReferenceName(const QVector<ProjectStructureEntry> &entries,
                                           ProjectStructureEntryKind kind)
{
    ReferenceKeyIndex entryKeys;
    for (const ProjectStructureEntry &entry : entries) {
        if (entry.kind != kind || entry.name.trimmed().isEmpty()) {
            continue;
        }

        const QString nodeKey = ProjectStructureIndex::structureEntryNodeKey(entry);
        entryKeys.keysByNameAndNamespace[namespacedReferenceLookupKey(entry.name, entry.namespacePath)].append(nodeKey);
    }

    return entryKeys;
}

ReferenceKeyIndex mergedReferenceKeysByReferenceName(const QVector<ProjectStructureEntry> &entries,
                                                     std::initializer_list<ProjectStructureEntryKind> kinds)
{
    ReferenceKeyIndex mergedKeys;
    for (const ProjectStructureEntryKind kind : kinds) {
        const ReferenceKeyIndex kindKeys = entryKeysByReferenceName(entries, kind);
        for (auto it = kindKeys.keysByNameAndNamespace.constBegin();
             it != kindKeys.keysByNameAndNamespace.constEnd();
             ++it) {
            mergedKeys.keysByNameAndNamespace[it.key()].append(it.value());
        }
    }
    return mergedKeys;
}

ReferenceResolution resolveCandidates(const QVector<QString> &entryKeys)
{
    if (entryKeys.size() == 1) {
        return ReferenceResolution{ReferenceResolutionState::Unique, entryKeys.first(), 1};
    }
    if (entryKeys.size() > 1) {
        return ReferenceResolution{ReferenceResolutionState::Ambiguous, QString(), static_cast<int>(entryKeys.size())};
    }

    return {};
}

ReferenceResolution resolveReferenceKey(const ReferenceKeyIndex &entryKeys,
                                        const QString &referenceName,
                                        const QString &ownerNamespacePath)
{
    const MapReferenceParts reference = parseMapReference(referenceName);
    if (reference.name.isEmpty()) {
        return {};
    }

    const QString targetNamespacePath = reference.hasNamespace
        ? namespacePathWithChild(ownerNamespacePath, reference.namespacePath)
        : ownerNamespacePath;

    return resolveCandidates(
        entryKeys.keysByNameAndNamespace.value(namespacedReferenceLookupKey(reference.name, targetNamespacePath)));
}

QString joinReferenceLookupToken(QString referenceName)
{
    referenceName = referenceName.trimmed();
    const int namespaceIndex = referenceName.indexOf(QLatin1Char('@'));
    const int endpointIndex = referenceName.indexOf(QLatin1Char(':'));
    if (endpointIndex >= 0 && (namespaceIndex < 0 || endpointIndex < namespaceIndex)) {
        const int removeLength = namespaceIndex < 0
            ? referenceName.size() - endpointIndex
            : namespaceIndex - endpointIndex;
        referenceName.remove(endpointIndex, removeLength);
    }
    return referenceName.trimmed();
}

bool joinReferenceShouldReportMissing(const QString &referenceName)
{
    const QString lookupName = joinReferenceLookupToken(referenceName);
    const QString nameOnly = mapReferenceLookupKey(lookupName);
    return lookupName.contains(QLatin1Char('@'))
        || lookupName.contains(QLatin1Char(':'))
        || nameOnly.endsWith(QStringLiteral(".s"))
        || nameOnly.endsWith(QStringLiteral(".m"));
}

QString mapCompositionReferenceToken(const TherionParsedLine &parsedLine)
{
    // Preview links describe spatial context, not map ownership, and commonly form cycles.
    if (parsedLine.directive == QStringLiteral("break")
        || parsedLine.directive == QStringLiteral("preview")) {
        return QString();
    }

    const QStringList &tokens = parsedLine.tokens;
    if (tokens.isEmpty()) {
        return QString();
    }

    const QString token = tokens.first().trimmed();
    if (token.isEmpty() || token.startsWith(QLatin1Char('-'))) {
        return QString();
    }

    if (tokens.size() == 1) {
        return token;
    }

    const QString previewMode = tokens.last().toLower();
    if (tokens.size() < 3
        || (previewMode != QStringLiteral("above")
            && previewMode != QStringLiteral("below")
            && previewMode != QStringLiteral("none"))) {
        return QString();
    }

    const QString firstOffsetToken = tokens.value(1).trimmed();
    const QString lastOffsetToken = tokens.value(tokens.size() - 2).trimmed();
    if (!firstOffsetToken.startsWith(QLatin1Char('['))
        || !lastOffsetToken.endsWith(QLatin1Char(']'))) {
        return QString();
    }

    return token;
}

QString mapPreviewReferenceToken(const TherionParsedLine &parsedLine)
{
    if (parsedLine.directive != QStringLiteral("preview")) {
        return QString();
    }

    const QString previewMode = parsedLine.tokens.value(1).trimmed().toLower();
    if (previewMode != QStringLiteral("above")
        && previewMode != QStringLiteral("below")
        && previewMode != QStringLiteral("none")) {
        return QString();
    }

    return parsedLine.tokens.value(2).trimmed();
}

MapCompositionContentKind mapCompositionContentKind(const QString &referenceName)
{
    const QString lookupName = mapReferenceLookupKey(referenceName);
    if (lookupName.endsWith(QStringLiteral(".m"))) {
        return MapCompositionContentKind::Map;
    }
    if (lookupName.endsWith(QStringLiteral(".s"))) {
        return MapCompositionContentKind::Scrap;
    }

    return MapCompositionContentKind::Unknown;
}

void appendMapReferenceDiagnostic(QVector<ProjectIndexDiagnostic> *diagnostics,
                                  ProjectIndexDiagnosticKind kind,
                                  const ProjectStructureEntry &mapEntry,
                                  const QString &sourceFile,
                                  int lineNumber,
                                  int columnNumber,
                                  int columnLength,
                                  const QString &referencedName,
                                  int candidateCount = 0)
{
    if (diagnostics == nullptr) {
        return;
    }

    ProjectIndexDiagnostic diagnostic;
    diagnostic.kind = kind;
    diagnostic.sourceObjectId = mapEntry.objectId;
    diagnostic.sourceFile = sourceFile;
    diagnostic.lineNumber = lineNumber;
    diagnostic.columnNumber = qMax(1, columnNumber);
    diagnostic.columnLength = qMax(0, columnLength);
    diagnostic.referencedName = referencedName;
    diagnostic.candidateCount = candidateCount;
    diagnostics->append(diagnostic);
}

void appendProjectIndexDiagnostic(QVector<ProjectIndexDiagnostic> *diagnostics,
                                  ProjectIndexDiagnosticKind kind,
                                  const QString &sourceObjectId,
                                  const QString &sourceFile,
                                  int lineNumber,
                                  int columnNumber,
                                  int columnLength,
                                  const QString &referencedName,
                                  int candidateCount = 0)
{
    if (diagnostics == nullptr) {
        return;
    }

    ProjectIndexDiagnostic diagnostic;
    diagnostic.kind = kind;
    diagnostic.sourceObjectId = sourceObjectId;
    diagnostic.sourceFile = sourceFile;
    diagnostic.lineNumber = lineNumber;
    diagnostic.columnNumber = qMax(1, columnNumber);
    diagnostic.columnLength = qMax(0, columnLength);
    diagnostic.referencedName = referencedName;
    diagnostic.candidateCount = candidateCount;
    diagnostics->append(diagnostic);
}

TherionSourcePhysicalRange parsedLineTokenPhysicalRange(const TherionParsedLine &parsedLine, int tokenIndex)
{
    TherionSourcePhysicalRange range;
    range.lineNumber = parsedLine.lineNumber;
    range.columnNumber = 1;
    range.columnLength = parsedLine.rawText.size();
    if (tokenIndex >= 0 && tokenIndex < parsedLine.tokenSpans.size()) {
        const TherionParsedToken &token = parsedLine.tokenSpans.at(tokenIndex);
        range.columnNumber = token.start + 1;
        range.columnLength = token.length;
    }
    return range;
}

ProjectStructureEntry nearestNamespaceEntryForLine(const QVector<ProjectStructureEntry> &entries,
                                                   const QString &sourceFile,
                                                   int lineNumber)
{
    ProjectStructureEntry nearestEntry;
    const QString normalizedSourceFile = normalizedFilePathKey(sourceFile);
    for (const ProjectStructureEntry &entry : entries) {
        if (normalizedFilePathKey(entry.sourceFile) != normalizedSourceFile
            || entry.lineNumber <= 0
            || entry.lineNumber > lineNumber) {
            continue;
        }
        if (nearestEntry.lineNumber <= 0
            || entry.lineNumber > nearestEntry.lineNumber
            || (entry.lineNumber == nearestEntry.lineNumber && entry.depth > nearestEntry.depth)) {
            nearestEntry = entry;
        }
    }
    return nearestEntry;
}

QString escapedIdentityToken(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    return value;
}

QString ProjectObjectIdentityGenerator::nextObjectId(const QString &category,
                                                     const QString &name,
                                                     const QString &sourceFile,
                                                     const QString &parentObjectId)
{
    const QString fileToken = escapedIdentityToken(normalizedFilePathKey(sourceFile));
    const QString parentToken = parentObjectId.isEmpty() ? QStringLiteral("root") : escapedIdentityToken(parentObjectId);
    const QString categoryToken = escapedIdentityToken(categoryIdentityToken(category));
    const QString nameToken = escapedIdentityToken(objectNameIdentityToken(name));
    const QString scopeKey = QStringList{fileToken, parentToken, categoryToken, nameToken}.join(QLatin1Char('|'));
    const int ordinal = siblingOrdinalsByScope.value(scopeKey, 0) + 1;
    siblingOrdinalsByScope.insert(scopeKey, ordinal);

    return QStringList{QStringLiteral("project-object"), fileToken, parentToken, categoryToken, nameToken, QString::number(ordinal)}
        .join(QLatin1Char('|'));
}

ProjectStructureEntryKind projectKindFromDirective(const QString &directive)
{
    const QString normalizedDirective = normalizedStructureDirective(directive);
    if (normalizedDirective == QStringLiteral("survey")) {
        return ProjectStructureEntryKind::Survey;
    }
    if (normalizedDirective == QStringLiteral("centreline")) {
        return ProjectStructureEntryKind::Centreline;
    }
    if (normalizedDirective == QStringLiteral("map")) {
        return ProjectStructureEntryKind::Map;
    }
    if (normalizedDirective == QStringLiteral("scrap")) {
        return ProjectStructureEntryKind::Scrap;
    }
    if (normalizedDirective == QStringLiteral("station")) {
        return ProjectStructureEntryKind::Station;
    }
    if (normalizedDirective == QStringLiteral("point")) {
        return ProjectStructureEntryKind::Point;
    }
    if (normalizedDirective == QStringLiteral("line")) {
        return ProjectStructureEntryKind::Line;
    }
    if (normalizedDirective == QStringLiteral("area")) {
        return ProjectStructureEntryKind::Area;
    }
    return ProjectStructureEntryKind::Unknown;
}

QString projectCategoryFromKind(ProjectStructureEntryKind kind)
{
    switch (kind) {
    case ProjectStructureEntryKind::Survey:
        return QStringLiteral("Surveys");
    case ProjectStructureEntryKind::Centreline:
        return QStringLiteral("Centrelines");
    case ProjectStructureEntryKind::Map:
        return QStringLiteral("Maps");
    case ProjectStructureEntryKind::Scrap:
        return QStringLiteral("Scraps");
    case ProjectStructureEntryKind::Station:
        return QStringLiteral("Stations");
    case ProjectStructureEntryKind::Point:
        return QStringLiteral("Points");
    case ProjectStructureEntryKind::Line:
        return QStringLiteral("Lines");
    case ProjectStructureEntryKind::Area:
        return QStringLiteral("Areas");
    case ProjectStructureEntryKind::Unknown:
        break;
    }

    return QString();
}

QString surveyNameFromLine(const TherionParsedLine &parsedLine)
{
    return parsedLine.tokens.value(1, QStringLiteral("survey"));
}

bool surveyCreatesNamespace(const TherionParsedLine &parsedLine)
{
    QString namespaceValue = commandOptionValue(parsedLine.tokens, QStringLiteral("namespace"));
    if (namespaceValue.isEmpty()) {
        namespaceValue = commandOptionValue(parsedLine.tokens, QStringLiteral("-namespace"));
    }
    return namespaceValue.compare(QStringLiteral("off"), Qt::CaseInsensitive) != 0;
}

QString normalizedStructureDirective(const QString &directive)
{
    if (directive == QStringLiteral("centerline")) {
        return QStringLiteral("centreline");
    }
    if (directive == QStringLiteral("endcenterline")) {
        return QStringLiteral("endcentreline");
    }

    return directive;
}

ProjectStructureEntryKind objectKindFromLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return ProjectStructureEntryKind::Unknown;
    }

    const QString directive = parsedLine.directive;
    if (directive == QStringLiteral("line")) {
        return ProjectStructureEntryKind::Line;
    }
    if (directive == QStringLiteral("area")) {
        return ProjectStructureEntryKind::Area;
    }
    if (directive == QStringLiteral("point")) {
        for (int index = 1; index < parsedLine.tokens.size(); ++index) {
            const QString token = parsedLine.tokens.at(index).toLower();
            if (token.startsWith(QLatin1Char('-'))) {
                continue;
            }
            if (token == QStringLiteral("station")) {
                return ProjectStructureEntryKind::Station;
            }
        }

        return ProjectStructureEntryKind::Point;
    }

    return ProjectStructureEntryKind::Unknown;
}

const TherionSourceLogicalDocument &logicalDocumentForFile(const QString &filePath,
                                                          ParsedFileCache *cache,
                                                          const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    const QString normalizedPath = normalizedFilePathKey(filePath);
    auto iterator = cache->logicalDocuments.find(normalizedPath);
    if (iterator != cache->logicalDocuments.end()) {
        return iterator.value();
    }

    QString fileContents;
    if (inMemoryFileContentsByPath.contains(normalizedPath)) {
        fileContents = inMemoryFileContentsByPath.value(normalizedPath);
    } else if (!DocumentFile::readUtf8TextFile(normalizedPath, &fileContents, nullptr)) {
        cache->logicalDocuments.insert(normalizedPath, logicalDocumentForText(QString()));
        return cache->logicalDocuments[normalizedPath];
    }

    cache->logicalDocuments.insert(normalizedPath, logicalDocumentForText(fileContents));
    return cache->logicalDocuments[normalizedPath];
}

bool isOpeningDirective(const QString &directive)
{
    const QString normalizedDirective = normalizedStructureDirective(directive);
    return normalizedDirective == QStringLiteral("survey")
        || normalizedDirective == QStringLiteral("centreline")
        || normalizedDirective == QStringLiteral("map")
        || normalizedDirective == QStringLiteral("scrap");
}

bool isClosingDirective(const QString &directive)
{
    const QString normalizedDirective = normalizedStructureDirective(directive);
    return normalizedDirective == QStringLiteral("endsurvey")
        || normalizedDirective == QStringLiteral("endcentreline")
        || normalizedDirective == QStringLiteral("endmap")
        || normalizedDirective == QStringLiteral("endscrap");
}

void closeBlock(QVector<ProjectBlock> *blockStack, ProjectStructureEntryKind kind, const QString &name = QString())
{
    if (blockStack->isEmpty()) {
        return;
    }

    if (name.isEmpty()) {
        while (!blockStack->isEmpty()) {
            const ProjectBlock block = blockStack->takeLast();
            if (block.kind == kind) {
                return;
            }
        }

        return;
    }

    for (int index = blockStack->size() - 1; index >= 0; --index) {
        const ProjectBlock &block = blockStack->at(index);
        if (block.kind == kind && block.name.compare(name, Qt::CaseInsensitive) == 0) {
            blockStack->resize(index);
            return;
        }
    }

    while (!blockStack->isEmpty()) {
        const ProjectBlock block = blockStack->takeLast();
        if (block.kind == kind) {
            return;
        }
    }
}

ProjectStructureEntry appendStructureEntry(QVector<ProjectStructureEntry> *entries,
                                           ProjectStructureEntryKind kind,
                                           const QString &name,
                                           const QString &sourceFile,
                                           int lineNumber,
                                           const QVector<ProjectBlock> &blockStack,
                                           ProjectObjectIdentityGenerator *identityGenerator,
                                           bool createsNamespace = true)
{
    ProjectStructureEntry entry;
    const QString category = projectCategoryFromKind(kind);
    entry.kind = kind;
    entry.parentObjectId = blockStack.isEmpty() ? QString() : blockStack.last().objectId;
    entry.objectId = identityGenerator->nextObjectId(category, name, sourceFile, entry.parentObjectId);
    entry.category = category;
    entry.name = name;
    const QString currentNamespacePath = blockStack.isEmpty() ? QString() : blockStack.last().namespacePath;
    entry.namespacePath = kind == ProjectStructureEntryKind::Survey && createsNamespace
        ? namespacePathWithChild(currentNamespacePath, name)
        : currentNamespacePath;
    entry.sourceFile = sourceFile;
    entry.lineNumber = lineNumber;
    entry.depth = blockStack.size();
    entry.createsNamespace = createsNamespace;
    entries->append(entry);
    return entry;
}

void appendProjectStructureFromFile(const QString &filePath,
                                    ParsedFileCache *cache,
                                    QVector<ProjectBlock> *blockStack,
                                    QSet<QString> *activeFiles,
                                    QVector<ProjectStructureEntry> *entries,
                                    ProjectObjectIdentityGenerator *identityGenerator,
                                    const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    const QString normalizedPath = normalizedFilePathKey(filePath);
    if (activeFiles->contains(normalizedPath)) {
        return;
    }

    activeFiles->insert(normalizedPath);
    const TherionSourceLogicalDocument &logicalDocument =
        logicalDocumentForFile(normalizedPath, cache, inMemoryFileContentsByPath);

    for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
        const TherionParsedLine &parsedLine = command.parsed;
        const QString directive = normalizedStructureDirective(command.metadata.commandName);

        if (directive == QStringLiteral("input") || directive == QStringLiteral("source")) {
            const QString inputTarget = parsedLine.tokens.value(1);
            const QString resolvedInputPath = resolveTherionSourceReferencePath(normalizedPath, inputTarget);
            if (!resolvedInputPath.isEmpty()) {
                appendProjectStructureFromFile(resolvedInputPath,
                                               cache,
                                               blockStack,
                                               activeFiles,
                                               entries,
                                               identityGenerator,
                                               inMemoryFileContentsByPath);
            }
            continue;
        }

        if (isClosingDirective(directive)) {
            const ProjectStructureEntryKind closingKind = projectKindFromDirective(directive.mid(3));
            const QString closingName = parsedLine.tokens.size() >= 2 ? parsedLine.tokens.value(1) : QString();
            closeBlock(blockStack, closingKind, closingName);
            continue;
        }

        const ProjectStructureEntryKind openingKind = projectKindFromDirective(directive);
        if (isOpeningDirective(directive)) {
            const QString openingName = directive == QStringLiteral("survey")
                ? surveyNameFromLine(parsedLine)
                : sectionNameFromLine(parsedLine);
            const bool createsNamespace = directive == QStringLiteral("survey") ? surveyCreatesNamespace(parsedLine) : true;
            const ProjectStructureEntry entry = appendStructureEntry(entries,
                                                                    openingKind,
                                                                    openingName,
                                                                    normalizedPath,
                                                                    command.startLineNumber,
                                                                    *blockStack,
                                                                    identityGenerator,
                                                                    createsNamespace);
            blockStack->append(ProjectBlock{openingKind, openingName, entry.objectId, entry.namespacePath, createsNamespace});
            continue;
        }

        const ProjectStructureEntryKind objectKind = objectKindFromLine(parsedLine);
        if (objectKind == ProjectStructureEntryKind::Unknown) {
            continue;
        }

        appendStructureEntry(entries,
                             objectKind,
                             objectNameFromLine(parsedLine),
                             normalizedPath,
                             command.startLineNumber,
                             *blockStack,
                             identityGenerator);
    }

    activeFiles->remove(normalizedPath);
}

QVector<QString> rootProjectFiles(const QVector<QString> &filePaths,
                                  ParsedFileCache *cache,
                                  const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    QSet<QString> includedFiles;
    for (const QString &filePath : filePaths) {
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(filePath, cache, inMemoryFileContentsByPath);
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            const TherionParsedLine &parsedLine = command.parsed;
            if (command.metadata.commandName != QStringLiteral("input")
                && command.metadata.commandName != QStringLiteral("source")) {
                continue;
            }

            const QString inputTarget = parsedLine.tokens.value(1);
            const QString resolvedInputPath = resolveTherionSourceReferencePath(filePath, inputTarget);
            if (!resolvedInputPath.isEmpty()) {
                includedFiles.insert(normalizedFilePathKey(resolvedInputPath));
            }
        }
    }

    QVector<QString> roots;
    for (const QString &filePath : filePaths) {
        if (!includedFiles.contains(normalizedFilePathKey(filePath))) {
            roots.append(filePath);
        }
    }

    if (roots.isEmpty()) {
        return filePaths;
    }

    return roots;
}

QString pointTypeToken(const TherionParsedLine &parsedLine)
{
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (TherionTokenRules::tokenStartsOption(token) || TherionTokenRules::isNumericToken(token)) {
            continue;
        }

        return token;
    }

    return QString();
}

QString objectNameFromLine(const TherionParsedLine &parsedLine)
{
    const QString directive = parsedLine.directive;

    if (directive == QStringLiteral("line") || directive == QStringLiteral("area")) {
        const QString identifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        if (!identifier.isEmpty()) {
            return identifier;
        }

        return parsedLine.tokens.value(1, directive);
    }

    if (directive == QStringLiteral("point")) {
        const QString stationName = commandOptionValue(parsedLine.tokens, QStringLiteral("-name"));
        if (!stationName.isEmpty()) {
            return stationName;
        }

        const QString identifier = commandOptionValue(parsedLine.tokens, QStringLiteral("-id"));
        if (!identifier.isEmpty()) {
            return identifier;
        }

        const QString pointType = pointTypeToken(parsedLine);
        if (!pointType.isEmpty()) {
            return pointType;
        }
    }

    return parsedLine.tokens.value(1, directive);
}

QString objectDisplayText(const ProjectStructureEntry &entry, const TherionParsedLine &parsedLine)
{
    if (entry.kind == ProjectStructureEntryKind::Line || entry.kind == ProjectStructureEntryKind::Area) {
        const QString subtype = parsedLine.tokens.value(1);
        if (!subtype.isEmpty() && subtype != entry.name) {
            return QStringLiteral("%1 (%2)").arg(subtype, entry.name);
        }
    }

    return entry.name;
}

QString sectionNameFromLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.size() >= 2) {
        return parsedLine.tokens.value(1);
    }

    return parsedLine.directive;
}

bool isInterestingProjectFile(const QFileInfo &fileInfo)
{
    const QString suffix = fileInfo.suffix().toLower();
    const QString fileName = fileInfo.fileName();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2")
        || isTherionConfigFileName(fileName);
}

bool isTherionConfigFile(const QFileInfo &fileInfo)
{
    return isTherionConfigFileName(fileInfo.fileName());
}

QString resolvePreferredProjectConfigPath(const QString &preferredConfigPath, const QString &projectRootPath)
{
    const QString trimmedPath = preferredConfigPath.trimmed();
    if (trimmedPath.isEmpty()) {
        return QString();
    }

    QFileInfo configInfo(trimmedPath);
    if (!configInfo.isAbsolute()) {
        configInfo = QFileInfo(QDir(projectRootPath).absoluteFilePath(trimmedPath));
    }
    if (!configInfo.exists() || !isTherionConfigFile(configInfo)) {
        return QString();
    }

    const QString normalizedProjectRoot = normalizedFilePathKey(projectRootPath);
    const QString normalizedConfigPath = normalizedFilePathKey(configInfo.absoluteFilePath());
    if (!pathIsInsideDirectory(normalizedConfigPath, normalizedProjectRoot)) {
        return QString();
    }

    return normalizedConfigPath;
}

RootConfigResolution rootConfigFiles(const QVector<QString> &filePaths,
                                     const QString &projectRootPath,
                                     const QString &preferredConfigPath)
{
    const QString normalizedProjectRoot = normalizedFilePathKey(projectRootPath);
    const QString normalizedPreferredConfigPath = resolvePreferredProjectConfigPath(preferredConfigPath, projectRootPath);
    if (!normalizedPreferredConfigPath.isEmpty()) {
        return RootConfigResolution{{normalizedPreferredConfigPath}, normalizedPreferredConfigPath, QString()};
    }

    QVector<QString> defaultConfigFiles;
    QVector<QString> namedConfigFiles;

    for (const QString &filePath : filePaths) {
        const QFileInfo fileInfo(filePath);
        if (!isTherionConfigFile(fileInfo)) {
            continue;
        }

        if (normalizedFilePathKey(fileInfo.dir().absolutePath()) != normalizedProjectRoot) {
            continue;
        }

        if (fileInfo.fileName().compare(QStringLiteral("thconfig"), Qt::CaseInsensitive) == 0) {
            defaultConfigFiles.append(filePath);
        } else {
            namedConfigFiles.append(filePath);
        }
    }

    if (!defaultConfigFiles.isEmpty()) {
        return RootConfigResolution{defaultConfigFiles, normalizedFilePathKey(defaultConfigFiles.first()), QString()};
    }
    if (namedConfigFiles.size() == 1) {
        return RootConfigResolution{namedConfigFiles, normalizedFilePathKey(namedConfigFiles.first()), QString()};
    }
    if (namedConfigFiles.isEmpty()) {
        return RootConfigResolution{{}, QString(), QString()};
    }

    return RootConfigResolution{
        {},
        QString(),
        QCoreApplication::translate("TherionStudio::ProjectStructureIndex",
                                    "Multiple Therion config files were found in the project root. Select a project target config in the Compiler pane to build the structure graph.")
    };
}

MapReferenceScanResult scanMapReferences(const QVector<ProjectStructureEntry> &entries,
                                         ParsedFileCache *cache,
                                         const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    MapReferenceScanResult result;

    const ReferenceKeyIndex mapKeysByName = entryKeysByReferenceName(entries, ProjectStructureEntryKind::Map);
    const ReferenceKeyIndex scrapKeysByName = entryKeysByReferenceName(entries, ProjectStructureEntryKind::Scrap);

    QHash<QString, QVector<ProjectStructureEntry>> mapsBySourceFile;
    for (const ProjectStructureEntry &entry : entries) {
        if (entry.kind == ProjectStructureEntryKind::Map && !entry.sourceFile.isEmpty() && entry.lineNumber > 0) {
            mapsBySourceFile[normalizedFilePathKey(entry.sourceFile)].append(entry);
        }
    }
    if (mapsBySourceFile.isEmpty()) {
        return result;
    }

    for (auto fileIt = mapsBySourceFile.constBegin(); fileIt != mapsBySourceFile.constEnd(); ++fileIt) {
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(fileIt.key(), cache, inMemoryFileContentsByPath);
        const QVector<TherionSourceLogicalCommand> &commands = logicalDocument.commands();
        if (commands.isEmpty()) {
            continue;
        }

        for (const ProjectStructureEntry &mapEntry : fileIt.value()) {
            int mapLineIndex = -1;
            for (int index = 0; index < commands.size(); ++index) {
                if (commands.at(index).startLineNumber == mapEntry.lineNumber
                    && commands.at(index).metadata.commandName == QStringLiteral("map")) {
                    mapLineIndex = index;
                    break;
                }
            }
            if (mapLineIndex < 0) {
                continue;
            }

            QSet<QString> scrapReferences;
            QSet<QString> childMapReferences;
            QSet<QString> previewReferences;
            bool sawMapContent = false;
            bool sawScrapContent = false;
            bool mixedContentDiagnosticAdded = false;
            auto registerContentKind = [&](MapCompositionContentKind contentKind,
                                           const TherionParsedLine &sourceLine,
                                           const QString &referenceName) {
                if (contentKind == MapCompositionContentKind::Unknown) {
                    return;
                }

                const bool mixedWithPreviousContent =
                    (contentKind == MapCompositionContentKind::Map && sawScrapContent)
                    || (contentKind == MapCompositionContentKind::Scrap && sawMapContent);
                if (mixedWithPreviousContent && !mixedContentDiagnosticAdded) {
                    const TherionSourcePhysicalRange referenceRange =
                        parsedLineTokenPhysicalRange(sourceLine, 0);
                    appendMapReferenceDiagnostic(&result.diagnostics,
                                                 ProjectIndexDiagnosticKind::MixedMapAndScrapReferences,
                                                 mapEntry,
                                                 fileIt.key(),
                                                 sourceLine.lineNumber,
                                                 referenceRange.columnNumber,
                                                 referenceRange.columnLength,
                                                 referenceName);
                    mixedContentDiagnosticAdded = true;
                }

                if (contentKind == MapCompositionContentKind::Map) {
                    sawMapContent = true;
                } else if (contentKind == MapCompositionContentKind::Scrap) {
                    sawScrapContent = true;
                }
            };

            int mapDepth = 0;
            for (int index = mapLineIndex; index < commands.size(); ++index) {
                const TherionSourceLogicalCommand &command = commands.at(index);
                const TherionParsedLine &parsedLine = command.parsed;
                const QString directive = command.metadata.commandName;

                if (directive == QStringLiteral("map")) {
                    ++mapDepth;
                    continue;
                }
                if (directive == QStringLiteral("endmap")) {
                    --mapDepth;
                    if (mapDepth <= 0) {
                        break;
                    }
                    continue;
                }
                if (mapDepth != 1) {
                    continue;
                }

                const QString previewReference = mapPreviewReferenceToken(parsedLine);
                if (!previewReference.isEmpty()) {
                    const ReferenceResolution previewResolution = resolveReferenceKey(mapKeysByName,
                                                                                      previewReference,
                                                                                      mapEntry.namespacePath);
                    if (previewResolution.state == ReferenceResolutionState::Unique) {
                        const QString owningMapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
                        if (previewResolution.key != owningMapKey) {
                            previewReferences.insert(previewResolution.key);
                        }
                    }
                    continue;
                }

                const QString candidate = mapCompositionReferenceToken(parsedLine);
                if (candidate.isEmpty()) {
                    continue;
                }
                const TherionSourcePhysicalRange candidateRange = parsedLineTokenPhysicalRange(parsedLine, 0);

                const ReferenceResolution childMapResolution = resolveReferenceKey(mapKeysByName, candidate, mapEntry.namespacePath);
                if (childMapResolution.state == ReferenceResolutionState::Unique) {
                    registerContentKind(MapCompositionContentKind::Map, parsedLine, candidate);
                    const QString owningMapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
                    if (childMapResolution.key != owningMapKey) {
                        childMapReferences.insert(childMapResolution.key);
                    }
                    continue;
                }
                if (childMapResolution.state == ReferenceResolutionState::Ambiguous) {
                    registerContentKind(MapCompositionContentKind::Map, parsedLine, candidate);
                    appendMapReferenceDiagnostic(&result.diagnostics,
                                                 ProjectIndexDiagnosticKind::AmbiguousMapReference,
                                                 mapEntry,
                                                 fileIt.key(),
                                                 parsedLine.lineNumber,
                                                 candidateRange.columnNumber,
                                                 candidateRange.columnLength,
                                                 candidate,
                                                 childMapResolution.candidateCount);
                    continue;
                }

                const ReferenceResolution scrapResolution = resolveReferenceKey(scrapKeysByName, candidate, mapEntry.namespacePath);
                if (scrapResolution.state == ReferenceResolutionState::Unique) {
                    registerContentKind(MapCompositionContentKind::Scrap, parsedLine, candidate);
                    scrapReferences.insert(scrapResolution.key);
                    continue;
                }
                if (scrapResolution.state == ReferenceResolutionState::Ambiguous) {
                    registerContentKind(MapCompositionContentKind::Scrap, parsedLine, candidate);
                    appendMapReferenceDiagnostic(&result.diagnostics,
                                                 ProjectIndexDiagnosticKind::AmbiguousMapScrapReference,
                                                 mapEntry,
                                                 fileIt.key(),
                                                 parsedLine.lineNumber,
                                                 candidateRange.columnNumber,
                                                 candidateRange.columnLength,
                                                 candidate,
                                                 scrapResolution.candidateCount);
                    continue;
                }

                const MapCompositionContentKind unresolvedContentKind = mapCompositionContentKind(candidate);
                if (unresolvedContentKind == MapCompositionContentKind::Unknown) {
                    continue;
                }

                registerContentKind(unresolvedContentKind, parsedLine, candidate);
                appendMapReferenceDiagnostic(&result.diagnostics,
                                             unresolvedContentKind == MapCompositionContentKind::Map
                                                 ? ProjectIndexDiagnosticKind::UnknownMapReference
                                                 : ProjectIndexDiagnosticKind::UnknownMapScrapReference,
                                             mapEntry,
                                             fileIt.key(),
                                             parsedLine.lineNumber,
                                             candidateRange.columnNumber,
                                             candidateRange.columnLength,
                                             candidate);
            }

            if (!scrapReferences.isEmpty()) {
                result.scrapReferencesByMapKey.insert(ProjectStructureIndex::structureEntryNodeKey(mapEntry), scrapReferences);
            }
            if (!childMapReferences.isEmpty()) {
                result.childMapReferencesByMapKey.insert(ProjectStructureIndex::structureEntryNodeKey(mapEntry), childMapReferences);
            }
            if (!previewReferences.isEmpty()) {
                result.previewReferencesByMapKey.insert(ProjectStructureIndex::structureEntryNodeKey(mapEntry), previewReferences);
            }
        }
    }

    return result;
}

QVector<ProjectIndexDiagnostic> scanJoinReferences(const QVector<ProjectStructureEntry> &entries,
                                                   ParsedFileCache *cache,
                                                   const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    QVector<ProjectIndexDiagnostic> diagnostics;
    const ReferenceKeyIndex joinObjectKeysByName = mergedReferenceKeysByReferenceName(
        entries,
        {ProjectStructureEntryKind::Scrap,
         ProjectStructureEntryKind::Line,
         ProjectStructureEntryKind::Point,
         ProjectStructureEntryKind::Station});

    QSet<QString> sourceFiles;
    for (const ProjectStructureEntry &entry : entries) {
        if (!entry.sourceFile.trimmed().isEmpty()) {
            sourceFiles.insert(normalizedFilePathKey(entry.sourceFile));
        }
    }

    for (const QString &sourceFile : std::as_const(sourceFiles)) {
        const TherionSourceLogicalDocument &logicalDocument =
            logicalDocumentForFile(sourceFile, cache, inMemoryFileContentsByPath);
        for (const TherionSourceLogicalCommand &command : logicalDocument.commands()) {
            if (command.metadata.commandName != QStringLiteral("join")) {
                continue;
            }

            const ProjectStructureEntry ownerEntry =
                nearestNamespaceEntryForLine(entries, sourceFile, command.startLineNumber);
            for (int tokenIndex = 1; tokenIndex < command.parsed.tokens.size(); ++tokenIndex) {
                const QString referenceName = command.parsed.tokens.value(tokenIndex).trimmed();
                if (referenceName.isEmpty() || referenceName.startsWith(QLatin1Char('-'))) {
                    break;
                }

                const QString lookupName = joinReferenceLookupToken(referenceName);
                if (lookupName.isEmpty()) {
                    continue;
                }

                const ReferenceResolution resolution =
                    resolveReferenceKey(joinObjectKeysByName, lookupName, ownerEntry.namespacePath);
                if (resolution.state == ReferenceResolutionState::Unique) {
                    continue;
                }

                const TherionSourcePhysicalRange referenceRange =
                    parsedLineTokenPhysicalRange(command.parsed, tokenIndex);
                if (resolution.state == ReferenceResolutionState::Ambiguous) {
                    appendProjectIndexDiagnostic(&diagnostics,
                                                 ProjectIndexDiagnosticKind::AmbiguousJoinReference,
                                                 ownerEntry.objectId,
                                                 sourceFile,
                                                 referenceRange.lineNumber,
                                                 referenceRange.columnNumber,
                                                 referenceRange.columnLength,
                                                 referenceName,
                                                 resolution.candidateCount);
                    continue;
                }

                if (!joinReferenceShouldReportMissing(referenceName)) {
                    continue;
                }
                appendProjectIndexDiagnostic(&diagnostics,
                                             ProjectIndexDiagnosticKind::UnknownJoinReference,
                                             ownerEntry.objectId,
                                             sourceFile,
                                             referenceRange.lineNumber,
                                             referenceRange.columnNumber,
                                             referenceRange.columnLength,
                                             referenceName);
            }
        }
    }

    return diagnostics;
}

QVector<ProjectStructureEntry> scanTh2ObjectsFromLogicalCommands(
    const QString &sourceFile,
    const QVector<TherionSourceLogicalCommand> &commands)
{
    QVector<ProjectStructureEntry> entries;
    if (commands.isEmpty()) {
        return entries;
    }

    QString currentScrapName;
    int currentScrapLine = 0;
    QString currentScrapObjectId;
    ProjectObjectIdentityGenerator identityGenerator;

    auto ensureScrap = [&](const QString &scrapName, int lineNumber) {
        if (!currentScrapObjectId.isEmpty()) {
            return currentScrapObjectId;
        }

        ProjectStructureEntry entry;
        entry.kind = ProjectStructureEntryKind::Scrap;
        entry.category = projectCategoryFromKind(entry.kind);
        entry.objectId = identityGenerator.nextObjectId(entry.category, scrapName, sourceFile, QString());
        entry.name = scrapName;
        entry.sourceFile = sourceFile;
        entry.lineNumber = lineNumber;
        entry.depth = 0;
        entries.append(entry);
        currentScrapObjectId = entry.objectId;
        return currentScrapObjectId;
    };

    for (const TherionSourceLogicalCommand &command : commands) {
        const TherionParsedLine &parsedLine = command.parsed;
        if (command.metadata.commandName == QStringLiteral("scrap")) {
            currentScrapName = parsedLine.tokens.value(1, QStringLiteral("Unnamed Scrap"));
            currentScrapLine = command.startLineNumber;
            currentScrapObjectId.clear();
            ensureScrap(currentScrapName, currentScrapLine);
            continue;
        }

        if (command.metadata.commandName == QStringLiteral("endscrap")) {
            currentScrapName.clear();
            currentScrapLine = 0;
            currentScrapObjectId.clear();
            continue;
        }

        const ProjectStructureEntryKind kind = objectKindFromLine(parsedLine);
        if (kind == ProjectStructureEntryKind::Unknown) {
            continue;
        }

        if (currentScrapName.isEmpty()) {
            currentScrapName = QObject::tr("Unassigned Objects");
            currentScrapLine = 0;
            currentScrapObjectId.clear();
            ensureScrap(currentScrapName, currentScrapLine);
        }
        const QString parentObjectId = ensureScrap(currentScrapName, currentScrapLine);

        ProjectStructureEntry entry;
        entry.kind = kind;
        entry.parentObjectId = parentObjectId;
        entry.category = projectCategoryFromKind(kind);
        entry.name = objectNameFromLine(parsedLine);
        entry.sourceFile = sourceFile;
        entry.lineNumber = command.startLineNumber;
        entry.depth = 1;

        if (entry.name.isEmpty()) {
            entry.name = objectDisplayText(entry, parsedLine);
        }
        entry.objectId = identityGenerator.nextObjectId(entry.category,
                                                        entry.name,
                                                        entry.sourceFile,
                                                        entry.parentObjectId);

        entries.append(entry);
    }

    return entries;
}
}

ProjectIndexSnapshot ProjectStructureIndex::scanProjectIndex(const QString &projectRootPath, QString *errorMessage)
{
    return scanProjectIndex(projectRootPath, QHash<QString, QString>(), errorMessage);
}

ProjectIndexSnapshot ProjectStructureIndex::scanProjectIndex(const QString &projectRootPath,
                                                             const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                             QString *errorMessage)
{
    return scanProjectIndex(projectRootPath, inMemoryFileContentsByPath, QString(), errorMessage);
}

ProjectIndexSnapshot ProjectStructureIndex::scanProjectIndex(const QString &projectRootPath,
                                                             const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                             const QString &preferredConfigPath,
                                                             QString *errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }

    ProjectIndexSnapshot snapshot;
    if (projectRootPath.isEmpty()) {
        return snapshot;
    }
    snapshot.projectRootPath = normalizedFilePathKey(projectRootPath);

    QDir projectRoot(projectRootPath);
    if (!projectRoot.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QCoreApplication::translate("TherionStudio::ProjectStructureIndex",
                                                        "The selected project folder does not exist.");
        }
        return snapshot;
    }

    QVector<QString> filePaths;
    QDirIterator iterator(projectRootPath,
                          {QStringLiteral("*.th"),
                           QStringLiteral("*.th2"),
                           QStringLiteral("*.thconfig"),
                           QStringLiteral("thconfig"),
                           QStringLiteral("thconfig.*")},
                          QDir::Files,
                          QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        const QString filePath = iterator.next();
        QFileInfo fileInfo(filePath);
        if (isInterestingProjectFile(fileInfo)) {
            filePaths.append(filePath);
        }
    }

    std::sort(filePaths.begin(), filePaths.end(), [](const QString &left, const QString &right) {
        return left.toLower() < right.toLower();
    });

    ParsedFileCache cache;
    const RootConfigResolution configResolution = rootConfigFiles(filePaths, projectRootPath, preferredConfigPath);
    snapshot.rootConfigPath = configResolution.configPath;
    if (!configResolution.errorMessage.isEmpty()) {
        if (errorMessage != nullptr) {
            *errorMessage = configResolution.errorMessage;
        }
        return snapshot;
    }

    QVector<QString> rootFiles = configResolution.rootFiles;
    if (rootFiles.isEmpty()) {
        rootFiles = rootProjectFiles(filePaths, &cache, inMemoryFileContentsByPath);
    }
    for (const QString &rootFile : std::as_const(rootFiles)) {
        snapshot.rootFilePaths.append(normalizedFilePathKey(rootFile));
    }

    QVector<ProjectBlock> blockStack;
    QSet<QString> activeFiles;
    ProjectObjectIdentityGenerator identityGenerator;
    for (const QString &filePath : rootFiles) {
        appendProjectStructureFromFile(filePath,
                                       &cache,
                                       &blockStack,
                                       &activeFiles,
                                       &snapshot.entries,
                                       &identityGenerator,
                                       inMemoryFileContentsByPath);
    }

    const MapReferenceScanResult mapReferenceScan = scanMapReferences(snapshot.entries,
                                                                      &cache,
                                                                      inMemoryFileContentsByPath);
    snapshot.mapScrapReferencesByMapKey = mapReferenceScan.scrapReferencesByMapKey;
    snapshot.mapChildReferencesByMapKey = mapReferenceScan.childMapReferencesByMapKey;
    snapshot.mapPreviewReferencesByMapKey = mapReferenceScan.previewReferencesByMapKey;
    snapshot.diagnostics = mapReferenceScan.diagnostics;
    snapshot.diagnostics += scanJoinReferences(snapshot.entries,
                                               &cache,
                                               inMemoryFileContentsByPath);
    return snapshot;
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanProject(const QString &projectRootPath, QString *errorMessage)
{
    return scanProject(projectRootPath, QHash<QString, QString>(), errorMessage);
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanProject(const QString &projectRootPath,
                                                                  const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                                  QString *errorMessage)
{
    return scanProjectIndex(projectRootPath, inMemoryFileContentsByPath, errorMessage).entries;
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanTh2Objects(const QString &sourceFile, const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return {};
    }

    return scanTh2ObjectsFromLogicalCommands(sourceFile, logicalDocumentForText(text).commands());
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanTh2Objects(const QString &sourceFile,
                                                                     const QVector<TherionParsedLine> &parsedLines)
{
    if (parsedLines.isEmpty()) {
        return {};
    }

    QVector<TherionSourceLogicalCommand> commands;
    commands.reserve(parsedLines.size());
    for (const TherionParsedLine &parsedLine : parsedLines) {
        TherionSourceLogicalCommand command;
        command.startLineNumber = parsedLine.lineNumber;
        command.parsed = parsedLine;
        command.metadata.commandName = parsedLine.directive;
        commands.append(command);
    }
    return scanTh2ObjectsFromLogicalCommands(sourceFile, commands);
}

QString ProjectStructureIndex::structureEntryNodeKey(const ProjectStructureEntry &entry)
{
    if (!entry.objectId.isEmpty()) {
        return entry.objectId;
    }

    return QStringLiteral("%1:%2").arg(normalizedFilePathKey(entry.sourceFile)).arg(entry.lineNumber);
}
}
