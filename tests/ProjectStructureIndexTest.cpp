#include "../src/core/ProjectStructureIndex.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <iostream>

using namespace TherionStudio;

namespace
{
QString normalizedPathForComparison(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }

    return condition;
}

bool writeTextFile(const QString &filePath, const QString &contents)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }

    return file.write(contents.toUtf8()) == contents.toUtf8().size();
}

int runProjectStructureHierarchyTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps")), "The temporary maps directory could not be created.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("root.th")),
                           QStringLiteral(
                               "survey cave\n"
                               "  input branch\n"
                               "  input maps/map.th2\n"
                               "endsurvey cave\n")),
                "The root Therion file could not be written.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("branch.th")),
                           QStringLiteral(
                               "survey branch -namespace off\n"
                               "  centreline\n"
                               "  endcentreline\n"
                               "endsurvey branch\n")),
                "The nested survey file could not be written.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("maps/map.th2")),
                           QStringLiteral(
                               "scrap s1\n"
                               "point 10 20 station -name 1@cave\n"
                               "point 1e2 -2.5E-1 altitude\n"
                               "line wall\n"
                               "endline\n"
                               "endscrap\n")),
                "The TH2 map file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const QVector<ProjectStructureEntry> entries = ProjectStructureIndex::scanProject(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    if (!expect(entries.size() == 7, "The survey hierarchy scan returned an unexpected number of entries.")) {
        return 1;
    }

    struct ExpectedEntry
    {
        QString category;
        QString name;
        int depth = 0;
        QString sourceFile;
        int lineNumber = 0;
        bool createsNamespace = true;
    };

    const QVector<ExpectedEntry> expectedEntries = {
        {QStringLiteral("Surveys"), QStringLiteral("cave"), 0, projectDir.filePath(QStringLiteral("root.th")), 1, true},
        {QStringLiteral("Surveys"), QStringLiteral("branch"), 1, projectDir.filePath(QStringLiteral("branch.th")), 1, false},
        {QStringLiteral("Centrelines"), QStringLiteral("centreline"), 2, projectDir.filePath(QStringLiteral("branch.th")), 2, true},
        {QStringLiteral("Scraps"), QStringLiteral("s1"), 1, projectDir.filePath(QStringLiteral("maps/map.th2")), 1, true},
        {QStringLiteral("Stations"), QStringLiteral("1@cave"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 2, true},
        {QStringLiteral("Points"), QStringLiteral("altitude"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 3, true},
        {QStringLiteral("Lines"), QStringLiteral("wall"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 4, true},
    };

    for (int index = 0; index < expectedEntries.size(); ++index) {
        const ProjectStructureEntry &entry = entries.at(index);
        const ExpectedEntry &expected = expectedEntries.at(index);

        if (!expect(entry.category == expected.category, "The survey hierarchy categories are out of order.")) {
            return 1;
        }
        if (!expect(entry.name == expected.name, "The survey hierarchy names are out of order.")) {
            return 1;
        }
        if (!expect(entry.depth == expected.depth, "The survey hierarchy nesting depth is incorrect.")) {
            return 1;
        }
        if (!expect(normalizedPathForComparison(entry.sourceFile) == normalizedPathForComparison(expected.sourceFile), "The survey hierarchy source file is incorrect.")) {
            return 1;
        }
        if (!expect(entry.lineNumber == expected.lineNumber, "The survey hierarchy line number is incorrect.")) {
            return 1;
        }
        if (!expect(entry.createsNamespace == expected.createsNamespace, "The survey namespace flag is incorrect.")) {
            return 1;
        }
    }

    return 0;
}

int runProjectIndexMapScrapReferenceTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps")), "The temporary maps directory could not be created.")) {
        return 1;
    }

    const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
    const QString mapFile = projectDir.filePath(QStringLiteral("maps/map.th2"));
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral(
                                  "survey cave\n"
                                  "  input maps/map.th2\n"
                                  "  map cave-map\n"
                                  "    stale-scrap\n"
                                  "  endmap\n"
                                  "endsurvey cave\n")),
                "The root Therion file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapFile,
                              QStringLiteral(
                                  "scrap s1\n"
                                  "endscrap\n")),
                "The TH2 map file could not be written.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(normalizedPathForComparison(rootFile),
                            QStringLiteral(
                                "survey cave\n"
                                "  input maps/map.th2\n"
                                "  map cave-map\n"
                                "    s1\n"
                                "  endmap\n"
                                "endsurvey cave\n"));

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                  inMemoryContents,
                                                                                  &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    ProjectStructureEntry mapEntry;
    bool foundMap = false;
    for (const ProjectStructureEntry &entry : snapshot.entries) {
        if (entry.category == QStringLiteral("Maps") && entry.name == QStringLiteral("cave-map")) {
            mapEntry = entry;
            foundMap = true;
            break;
        }
    }
    if (!expect(foundMap, "The project index did not find the map entry.")) {
        return 1;
    }

    const QString mapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
    const QSet<QString> scrapReferences = snapshot.mapScrapReferencesByMapKey.value(mapKey);
    if (!expect(scrapReferences.contains(QStringLiteral("s1")),
                "The project index did not resolve the map-to-scrap reference from in-memory source text.")) {
        return 1;
    }
    if (!expect(!scrapReferences.contains(QStringLiteral("stale-scrap")),
                "The project index used stale on-disk source text for map-to-scrap references.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runProjectStructureHierarchyTest() != 0) {
        return 1;
    }
    if (runProjectIndexMapScrapReferenceTest() != 0) {
        return 1;
    }

    return 0;
}
