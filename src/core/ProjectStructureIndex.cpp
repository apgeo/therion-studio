#include "ProjectStructureIndex.h"

#include "DocumentFile.h"
#include "TherionDocumentParser.h"

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
    QString category;
    QString name;
    bool createsNamespace = true;
};

struct ParsedFileCache
{
    QHash<QString, QVector<TherionParsedLine>> parsedLines;
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
QString objectCategoryFromLine(const TherionParsedLine &parsedLine);
QString objectNameFromLine(const TherionParsedLine &parsedLine);

QString projectCategoryFromDirective(const QString &directive)
{
    if (directive == QStringLiteral("survey")) {
        return QStringLiteral("Surveys");
    }
    if (directive == QStringLiteral("centreline")) {
        return QStringLiteral("Centrelines");
    }
    if (directive == QStringLiteral("map")) {
        return QStringLiteral("Maps");
    }
    if (directive == QStringLiteral("scrap")) {
        return QStringLiteral("Scraps");
    }
    if (directive == QStringLiteral("station")) {
        return QStringLiteral("Stations");
    }
    if (directive == QStringLiteral("point")) {
        return QStringLiteral("Points");
    }
    if (directive == QStringLiteral("line")) {
        return QStringLiteral("Lines");
    }
    if (directive == QStringLiteral("area")) {
        return QStringLiteral("Areas");
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

QString objectCategoryFromLine(const TherionParsedLine &parsedLine)
{
    if (parsedLine.tokens.isEmpty()) {
        return QString();
    }

    const QString directive = parsedLine.directive;
    if (directive == QStringLiteral("line")) {
        return QStringLiteral("Lines");
    }
    if (directive == QStringLiteral("area")) {
        return QStringLiteral("Areas");
    }
    if (directive == QStringLiteral("point")) {
        for (int index = 1; index < parsedLine.tokens.size(); ++index) {
            const QString token = parsedLine.tokens.at(index).toLower();
            if (token.startsWith(QLatin1Char('-'))) {
                continue;
            }
            if (token == QStringLiteral("station")) {
                return QStringLiteral("Stations");
            }
        }

        return QStringLiteral("Points");
    }

    return QString();
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

void closeBlock(QVector<ProjectBlock> *blockStack, const QString &category, const QString &name = QString())
{
    if (blockStack->isEmpty()) {
        return;
    }

    if (name.isEmpty()) {
        while (!blockStack->isEmpty()) {
            const ProjectBlock block = blockStack->takeLast();
            if (block.category == category) {
                return;
            }
        }

        return;
    }

    for (int index = blockStack->size() - 1; index >= 0; --index) {
        const ProjectBlock &block = blockStack->at(index);
        if (block.category == category && block.name.compare(name, Qt::CaseInsensitive) == 0) {
            blockStack->resize(index);
            return;
        }
    }

    while (!blockStack->isEmpty()) {
        const ProjectBlock block = blockStack->takeLast();
        if (block.category == category) {
            return;
        }
    }
}

void appendStructureEntry(QVector<ProjectStructureEntry> *entries,
                          const QString &category,
                          const QString &name,
                          const QString &sourceFile,
                          int lineNumber,
                          const QVector<ProjectBlock> &blockStack,
                          bool createsNamespace = true)
{
    ProjectStructureEntry entry;
    entry.category = category;
    entry.name = name;
    entry.sourceFile = sourceFile;
    entry.lineNumber = lineNumber;
    entry.depth = blockStack.size();
    entry.createsNamespace = createsNamespace;
    entries->append(entry);
}

void appendProjectStructureFromFile(const QString &filePath,
                                    ParsedFileCache *cache,
                                    QVector<ProjectBlock> *blockStack,
                                    QSet<QString> *activeFiles,
                                    QVector<ProjectStructureEntry> *entries,
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
                appendProjectStructureFromFile(resolvedInputPath, cache, blockStack, activeFiles, entries, inMemoryFileContentsByPath);
            }
            continue;
        }

        if (isClosingDirective(directive)) {
            const QString closingCategory = projectCategoryFromDirective(directive.mid(3));
            const QString closingName = parsedLine.tokens.size() >= 2 ? parsedLine.tokens.value(1) : QString();
            closeBlock(blockStack, closingCategory, closingName);
            continue;
        }

        const QString openingCategory = projectCategoryFromDirective(directive);
        if (isOpeningDirective(directive)) {
            const QString openingName = directive == QStringLiteral("survey")
                ? surveyNameFromLine(parsedLine)
                : sectionNameFromLine(parsedLine);
            const bool createsNamespace = directive == QStringLiteral("survey") ? surveyCreatesNamespace(parsedLine) : true;
            appendStructureEntry(entries, openingCategory, openingName, normalizedPath, parsedLine.lineNumber, *blockStack, createsNamespace);
            blockStack->append(ProjectBlock{openingCategory, openingName, createsNamespace});
            continue;
        }

        const QString objectCategory = objectCategoryFromLine(parsedLine);
        if (objectCategory.isEmpty()) {
            continue;
        }

        appendStructureEntry(entries, objectCategory, objectNameFromLine(parsedLine), normalizedPath, parsedLine.lineNumber, *blockStack);
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
    if (entry.category == QStringLiteral("Lines") || entry.category == QStringLiteral("Areas")) {
        const QString subtype = parsedLine.tokens.value(1);
        if (!subtype.isEmpty() && subtype != entry.name) {
            return QStringLiteral("%1 (%2)").arg(subtype, entry.name);
        }
    }

    return entry.name;
}

QString sectionEndDirectiveForCategory(const QString &category)
{
    if (category == QStringLiteral("Surveys")) {
        return QStringLiteral("endsurvey");
    }
    if (category == QStringLiteral("Centrelines")) {
        return QStringLiteral("endcentreline");
    }
    if (category == QStringLiteral("Maps")) {
        return QStringLiteral("endmap");
    }
    if (category == QStringLiteral("Scraps")) {
        return QStringLiteral("endscrap");
    }

    return QString();
}

bool isSectionCategory(const QString &category)
{
    return category == QStringLiteral("Surveys")
    || category == QStringLiteral("Centrelines")
        || category == QStringLiteral("Maps")
        || category == QStringLiteral("Scraps");
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
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanProject(const QString &projectRootPath, QString *errorMessage)
{
    return scanProject(projectRootPath, QHash<QString, QString>(), errorMessage);
}

QVector<ProjectStructureEntry> ProjectStructureIndex::scanProject(const QString &projectRootPath,
                                                                  const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                                  QString *errorMessage)
{
    QVector<ProjectStructureEntry> entries;
    if (projectRootPath.isEmpty()) {
        return entries;
    }

    QDir projectRoot(projectRootPath);
    if (!projectRoot.exists()) {
        if (errorMessage != nullptr) {
            *errorMessage = QStringLiteral("The selected project folder does not exist.");
        }
        return entries;
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
    for (const QString &filePath : rootFiles) {
        appendProjectStructureFromFile(filePath, &cache, &blockStack, &activeFiles, &entries, inMemoryFileContentsByPath);
    }

    return entries;
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

    auto ensureScrap = [&](const QString &scrapName, int lineNumber) {
        if (!entries.isEmpty() && entries.last().category == QStringLiteral("Scraps") && entries.last().name == scrapName) {
            return;
        }

        entries.append(ProjectStructureEntry{QStringLiteral("Scraps"), scrapName, sourceFile, lineNumber, 0});
    };

    for (const TherionParsedLine &parsedLine : parsedLines) {
        if (parsedLine.directive == QStringLiteral("scrap")) {
            currentScrapName = parsedLine.tokens.value(1, QStringLiteral("Unnamed Scrap"));
            currentScrapLine = parsedLine.lineNumber;
            ensureScrap(currentScrapName, currentScrapLine);
            continue;
        }

        if (parsedLine.directive == QStringLiteral("endscrap")) {
            currentScrapName.clear();
            currentScrapLine = 0;
            continue;
        }

        const QString category = objectCategoryFromLine(parsedLine);
        if (category.isEmpty()) {
            continue;
        }

        if (currentScrapName.isEmpty()) {
            currentScrapName = QObject::tr("Unassigned Objects");
            currentScrapLine = 0;
            ensureScrap(currentScrapName, currentScrapLine);
        }

        ProjectStructureEntry entry;
        entry.category = category;
        entry.name = objectNameFromLine(parsedLine);
        entry.sourceFile = sourceFile;
        entry.lineNumber = parsedLine.lineNumber;
        entry.depth = 1;

        if (entry.name.isEmpty()) {
            entry.name = objectDisplayText(entry, parsedLine);
        }

        entries.append(entry);
    }

    return entries;
}
}
