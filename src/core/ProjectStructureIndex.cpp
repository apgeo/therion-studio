#include "ProjectStructureIndex.h"

#include "DocumentFile.h"
#include "TherionDocumentParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
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
    bool createsNamespace = true;
};

struct ParsedFileCache
{
    QHash<QString, QVector<TherionParsedLine>> parsedLines;
};

struct ProjectObjectIdentityGenerator
{
    QHash<QString, int> siblingOrdinalsByScope;

    QString nextObjectId(const QString &category,
                         const QString &name,
                         const QString &sourceFile,
                         const QString &parentObjectId);
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

QString optionValue(const QStringList &tokens, const QString &option);
QString sectionNameFromLine(const TherionParsedLine &parsedLine);
ProjectStructureEntryKind objectKindFromLine(const TherionParsedLine &parsedLine);
QString objectNameFromLine(const TherionParsedLine &parsedLine);

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
    if (directive == QStringLiteral("survey")) {
        return ProjectStructureEntryKind::Survey;
    }
    if (directive == QStringLiteral("centreline")) {
        return ProjectStructureEntryKind::Centreline;
    }
    if (directive == QStringLiteral("map")) {
        return ProjectStructureEntryKind::Map;
    }
    if (directive == QStringLiteral("scrap")) {
        return ProjectStructureEntryKind::Scrap;
    }
    if (directive == QStringLiteral("station")) {
        return ProjectStructureEntryKind::Station;
    }
    if (directive == QStringLiteral("point")) {
        return ProjectStructureEntryKind::Point;
    }
    if (directive == QStringLiteral("line")) {
        return ProjectStructureEntryKind::Line;
    }
    if (directive == QStringLiteral("area")) {
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
    QString namespaceValue = optionValue(parsedLine.tokens, QStringLiteral("namespace"));
    if (namespaceValue.isEmpty()) {
        namespaceValue = optionValue(parsedLine.tokens, QStringLiteral("-namespace"));
    }
    return namespaceValue.compare(QStringLiteral("off"), Qt::CaseInsensitive) != 0;
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

QString optionValue(const QStringList &tokens, const QString &option)
{
    const QString normalizedOption = option.toLower();
    for (int index = 0; index + 1 < tokens.size(); ++index) {
        if (tokens.at(index).toLower() != normalizedOption) {
            continue;
        }

        const QString candidate = tokens.at(index + 1);
        if (!candidate.startsWith(QLatin1Char('-'))) {
            return candidate;
        }
    }

    return QString();
}

QString resolveInputPath(const QString &currentFilePath, const QString &inputPath)
{
    if (inputPath.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo currentFileInfo(currentFilePath);
    const QDir currentDirectory = currentFileInfo.dir();

    const auto normalizeCandidate = [](const QFileInfo &fileInfo) {
        return fileInfo.exists() ? fileInfo.absoluteFilePath() : QString();
    };

    const QFileInfo relativeCandidate(currentDirectory.filePath(inputPath));
    QString resolvedPath = normalizeCandidate(relativeCandidate);
    if (!resolvedPath.isEmpty()) {
        return resolvedPath;
    }

    const QFileInfo absoluteCandidate(inputPath);
    if (absoluteCandidate.isAbsolute()) {
        resolvedPath = normalizeCandidate(absoluteCandidate);
        if (!resolvedPath.isEmpty()) {
            return resolvedPath;
        }
    }

    if (QFileInfo(inputPath).suffix().isEmpty()) {
        const QFileInfo defaultExtensionCandidate(currentDirectory.filePath(inputPath + QStringLiteral(".th")));
        resolvedPath = normalizeCandidate(defaultExtensionCandidate);
        if (!resolvedPath.isEmpty()) {
            return resolvedPath;
        }
    }

    return QString();
}

const QVector<TherionParsedLine> &parsedLinesForFile(const QString &filePath,
                                                     ParsedFileCache *cache,
                                                     const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    const QString normalizedPath = normalizedFilePathKey(filePath);
    auto iterator = cache->parsedLines.find(normalizedPath);
    if (iterator != cache->parsedLines.end()) {
        return iterator.value();
    }

    QString fileContents;
    if (inMemoryFileContentsByPath.contains(normalizedPath)) {
        fileContents = inMemoryFileContentsByPath.value(normalizedPath);
    } else if (!DocumentFile::readUtf8TextFile(normalizedPath, &fileContents, nullptr)) {
        cache->parsedLines.insert(normalizedPath, {});
        return cache->parsedLines[normalizedPath];
    }

    cache->parsedLines.insert(normalizedPath, TherionDocumentParser::parseText(fileContents));
    return cache->parsedLines[normalizedPath];
}

bool isOpeningDirective(const QString &directive)
{
    return directive == QStringLiteral("survey")
        || directive == QStringLiteral("centreline")
        || directive == QStringLiteral("map")
        || directive == QStringLiteral("scrap");
}

bool isClosingDirective(const QString &directive)
{
    return directive == QStringLiteral("endsurvey")
        || directive == QStringLiteral("endcentreline")
        || directive == QStringLiteral("endmap")
        || directive == QStringLiteral("endscrap");
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
    const QVector<TherionParsedLine> &parsedLines = parsedLinesForFile(normalizedPath, cache, inMemoryFileContentsByPath);

    for (const TherionParsedLine &parsedLine : parsedLines) {
        const QString directive = parsedLine.directive;

        if (directive == QStringLiteral("input")) {
            const QString inputTarget = parsedLine.tokens.value(1);
            const QString resolvedInputPath = resolveInputPath(normalizedPath, inputTarget);
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
                                                                    parsedLine.lineNumber,
                                                                    *blockStack,
                                                                    identityGenerator,
                                                                    createsNamespace);
            blockStack->append(ProjectBlock{openingKind, openingName, entry.objectId, createsNamespace});
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
                             parsedLine.lineNumber,
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
        const QVector<TherionParsedLine> &parsedLines = parsedLinesForFile(filePath, cache, inMemoryFileContentsByPath);
        for (const TherionParsedLine &parsedLine : parsedLines) {
            if (parsedLine.directive != QStringLiteral("input")) {
                continue;
            }

            const QString inputTarget = parsedLine.tokens.value(1);
            const QString resolvedInputPath = resolveInputPath(filePath, inputTarget);
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

bool tokenLooksNumeric(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    static const QRegularExpression numericPattern(
        QStringLiteral(R"(^[+-]?(?:(?:\d+(?:\.\d*)?)|(?:\.\d+))(?:[eE][+-]?\d+)?$)"));
    return numericPattern.match(token).hasMatch();
}

QString pointTypeToken(const TherionParsedLine &parsedLine)
{
    for (int index = 1; index < parsedLine.tokens.size(); ++index) {
        const QString token = parsedLine.tokens.at(index);
        if (token.startsWith(QLatin1Char('-')) || tokenLooksNumeric(token)) {
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
        const QString identifier = optionValue(parsedLine.tokens, QStringLiteral("-id"));
        if (!identifier.isEmpty()) {
            return identifier;
        }

        return parsedLine.tokens.value(1, directive);
    }

    if (directive == QStringLiteral("point")) {
        const QString stationName = optionValue(parsedLine.tokens, QStringLiteral("-name"));
        if (!stationName.isEmpty()) {
            return stationName;
        }

        const QString identifier = optionValue(parsedLine.tokens, QStringLiteral("-id"));
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
    return suffix == QStringLiteral("th") || suffix == QStringLiteral("th2");
}

QHash<QString, QSet<QString>> mapScrapReferencesByMapKey(const QVector<ProjectStructureEntry> &entries,
                                                         ParsedFileCache *cache,
                                                         const QHash<QString, QString> &inMemoryFileContentsByPath)
{
    QHash<QString, QSet<QString>> referencesByMap;

    QSet<QString> knownScrapNames;
    for (const ProjectStructureEntry &entry : entries) {
        if (entry.kind == ProjectStructureEntryKind::Scrap && !entry.name.trimmed().isEmpty()) {
            knownScrapNames.insert(entry.name.trimmed().toLower());
        }
    }
    if (knownScrapNames.isEmpty()) {
        return referencesByMap;
    }

    QHash<QString, QVector<ProjectStructureEntry>> mapsBySourceFile;
    for (const ProjectStructureEntry &entry : entries) {
        if (entry.kind == ProjectStructureEntryKind::Map && !entry.sourceFile.isEmpty() && entry.lineNumber > 0) {
            mapsBySourceFile[normalizedFilePathKey(entry.sourceFile)].append(entry);
        }
    }
    if (mapsBySourceFile.isEmpty()) {
        return referencesByMap;
    }

    for (auto fileIt = mapsBySourceFile.constBegin(); fileIt != mapsBySourceFile.constEnd(); ++fileIt) {
        const QVector<TherionParsedLine> &parsedLines = parsedLinesForFile(fileIt.key(), cache, inMemoryFileContentsByPath);
        if (parsedLines.isEmpty()) {
            continue;
        }

        for (const ProjectStructureEntry &mapEntry : fileIt.value()) {
            int mapLineIndex = -1;
            for (int index = 0; index < parsedLines.size(); ++index) {
                if (parsedLines.at(index).lineNumber == mapEntry.lineNumber
                    && parsedLines.at(index).directive == QStringLiteral("map")) {
                    mapLineIndex = index;
                    break;
                }
            }
            if (mapLineIndex < 0) {
                continue;
            }

            QSet<QString> scrapReferences;
            int mapDepth = 0;
            for (int index = mapLineIndex; index < parsedLines.size(); ++index) {
                const TherionParsedLine &parsedLine = parsedLines.at(index);
                const QString directive = parsedLine.directive;

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
                if (mapDepth != 1 || parsedLine.tokens.size() != 1) {
                    continue;
                }

                const QString candidate = parsedLine.tokens.first().trimmed().toLower();
                if (candidate.isEmpty() || candidate.startsWith(QLatin1Char('-'))) {
                    continue;
                }
                if (knownScrapNames.contains(candidate)) {
                    scrapReferences.insert(candidate);
                }
            }

            if (!scrapReferences.isEmpty()) {
                referencesByMap.insert(ProjectStructureIndex::structureEntryNodeKey(mapEntry), scrapReferences);
            }
        }
    }

    return referencesByMap;
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
    ProjectIndexSnapshot snapshot;
    if (projectRootPath.isEmpty()) {
        return snapshot;
    }

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
                          {QStringLiteral("*.th"), QStringLiteral("*.th2")},
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
    const QVector<QString> rootFiles = rootProjectFiles(filePaths, &cache, inMemoryFileContentsByPath);

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

    snapshot.mapScrapReferencesByMapKey = mapScrapReferencesByMapKey(snapshot.entries, &cache, inMemoryFileContentsByPath);
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
    QVector<ProjectStructureEntry> entries;
    if (text.trimmed().isEmpty()) {
        return entries;
    }

    const QVector<TherionParsedLine> parsedLines = TherionDocumentParser::parseText(text);
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

    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive == QStringLiteral("scrap")) {
            currentScrapName = parsedLine.tokens.value(1, QStringLiteral("Unnamed Scrap"));
            currentScrapLine = parsedLine.lineNumber;
            currentScrapObjectId.clear();
            ensureScrap(currentScrapName, currentScrapLine);
            continue;
        }

        if (parsedLine.directive == QStringLiteral("endscrap")) {
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
        entry.lineNumber = parsedLine.lineNumber;
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

QString ProjectStructureIndex::structureEntryNodeKey(const ProjectStructureEntry &entry)
{
    if (!entry.objectId.isEmpty()) {
        return entry.objectId;
    }

    return QStringLiteral("%1:%2").arg(normalizedFilePathKey(entry.sourceFile)).arg(entry.lineNumber);
}
}
