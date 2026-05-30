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
        ProjectStructureEntryKind kind = ProjectStructureEntryKind::Unknown;
        QString category;
        QString name;
        int depth = 0;
        QString sourceFile;
        int lineNumber = 0;
        bool createsNamespace = true;
    };

    const QVector<ExpectedEntry> expectedEntries = {
        {ProjectStructureEntryKind::Survey, QStringLiteral("Surveys"), QStringLiteral("cave"), 0, projectDir.filePath(QStringLiteral("root.th")), 1, true},
        {ProjectStructureEntryKind::Survey, QStringLiteral("Surveys"), QStringLiteral("branch"), 1, projectDir.filePath(QStringLiteral("branch.th")), 1, false},
        {ProjectStructureEntryKind::Centreline, QStringLiteral("Centrelines"), QStringLiteral("centreline"), 2, projectDir.filePath(QStringLiteral("branch.th")), 2, true},
        {ProjectStructureEntryKind::Scrap, QStringLiteral("Scraps"), QStringLiteral("s1"), 1, projectDir.filePath(QStringLiteral("maps/map.th2")), 1, true},
        {ProjectStructureEntryKind::Station, QStringLiteral("Stations"), QStringLiteral("1@cave"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 2, true},
        {ProjectStructureEntryKind::Point, QStringLiteral("Points"), QStringLiteral("altitude"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 3, true},
        {ProjectStructureEntryKind::Line, QStringLiteral("Lines"), QStringLiteral("wall"), 2, projectDir.filePath(QStringLiteral("maps/map.th2")), 4, true},
    };

    for (int index = 0; index < expectedEntries.size(); ++index) {
        const ProjectStructureEntry &entry = entries.at(index);
        const ExpectedEntry &expected = expectedEntries.at(index);

        if (!expect(entry.kind == expected.kind, "The survey hierarchy kinds are out of order.")) {
            return 1;
        }
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
        if (!expect(!entry.objectId.isEmpty(), "The survey hierarchy entry object ID should not be empty.")) {
            return 1;
        }
        if (!expect((expected.depth == 0) == entry.parentObjectId.isEmpty(),
                    "The survey hierarchy parent object ID does not match the nesting depth.")) {
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
                                "    missing-scrap\n"
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
        if (entry.kind == ProjectStructureEntryKind::Map && entry.name == QStringLiteral("cave-map")) {
            mapEntry = entry;
            foundMap = true;
            break;
        }
    }
    if (!expect(foundMap, "The project index did not find the map entry.")) {
        return 1;
    }
    if (!expect(!mapEntry.objectId.isEmpty(), "The project index map entry should expose a stable object ID.")) {
        return 1;
    }

    const QString mapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
    if (!expect(mapKey == mapEntry.objectId, "The structure node key should prefer the stable object ID.")) {
        return 1;
    }
    const QSet<QString> scrapReferences = snapshot.mapScrapReferencesByMapKey.value(mapKey);
    if (!expect(scrapReferences.contains(QStringLiteral("s1")),
                "The project index did not resolve the map-to-scrap reference from in-memory source text.")) {
        return 1;
    }
    if (!expect(!scrapReferences.contains(QStringLiteral("stale-scrap")),
                "The project index used stale on-disk source text for map-to-scrap references.")) {
        return 1;
    }
    if (!expect(snapshot.diagnostics.size() == 1,
                "The project index should report one unresolved map scrap reference.")) {
        return 1;
    }
    const ProjectIndexDiagnostic &diagnostic = snapshot.diagnostics.first();
    if (!expect(diagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapScrapReference,
                "The project index reported an unexpected diagnostic kind.")) {
        return 1;
    }
    if (!expect(diagnostic.sourceObjectId == mapEntry.objectId,
                "The unresolved map scrap reference diagnostic should point at the owning map object.")) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(diagnostic.sourceFile) == normalizedPathForComparison(rootFile),
                "The unresolved map scrap reference diagnostic source file is incorrect.")) {
        return 1;
    }
    if (!expect(diagnostic.lineNumber == 5,
                "The unresolved map scrap reference diagnostic line number is incorrect.")) {
        return 1;
    }
    if (!expect(diagnostic.referencedName == QStringLiteral("missing-scrap"),
                "The unresolved map scrap reference diagnostic target name is incorrect.")) {
        return 1;
    }

    inMemoryContents.insert(normalizedPathForComparison(rootFile),
                            QStringLiteral(
                                "\n"
                                "survey cave\n"
                                "  input maps/map.th2\n"
                                "  map cave-map\n"
                                "    s1\n"
                                "    missing-scrap\n"
                                "  endmap\n"
                                "endsurvey cave\n"));
    const ProjectIndexSnapshot shiftedSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                         inMemoryContents,
                                                                                         &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    ProjectStructureEntry shiftedMapEntry;
    bool foundShiftedMap = false;
    for (const ProjectStructureEntry &entry : shiftedSnapshot.entries) {
        if (entry.kind == ProjectStructureEntryKind::Map && entry.name == QStringLiteral("cave-map")) {
            shiftedMapEntry = entry;
            foundShiftedMap = true;
            break;
        }
    }
    if (!expect(foundShiftedMap, "The shifted project index did not find the map entry.")) {
        return 1;
    }
    if (!expect(shiftedMapEntry.objectId == mapEntry.objectId,
                "The project index object ID should stay stable when source line numbers shift.")) {
        return 1;
    }
    if (!expect(shiftedSnapshot.diagnostics.size() == 1
                    && shiftedSnapshot.diagnostics.first().sourceObjectId == shiftedMapEntry.objectId,
                "The shifted project index diagnostic should stay attached to the map object ID.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexThconfigSourceGraphTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("nested")), "The temporary nested project directory could not be created.")) {
        return 1;
    }

    const QString configFile = projectDir.filePath(QStringLiteral("thconfig"));
    const QString sourceFile = projectDir.filePath(QStringLiteral("main.th"));
    const QString nestedConfigFile = projectDir.filePath(QStringLiteral("nested/nested.thconfig"));
    const QString nestedSourceFile = projectDir.filePath(QStringLiteral("nested/nested.th"));
    if (!expect(writeTextFile(configFile,
                              QStringLiteral(
                                  "source main\n")),
                "The temporary thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(sourceFile,
                              QStringLiteral(
                                  "survey from-config\n"
                                  "endsurvey from-config\n")),
                "The temporary thconfig source file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(nestedConfigFile,
                              QStringLiteral(
                                  "source nested.th\n")),
                "The temporary nested thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(nestedSourceFile,
                              QStringLiteral(
                                  "survey nested-config\n"
                                  "endsurvey nested-config\n")),
                "The temporary nested thconfig source file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(snapshot.entries.size() == 1, "The thconfig source graph scan returned an unexpected entry count.")) {
        return 1;
    }

    const ProjectStructureEntry &entry = snapshot.entries.first();
    if (!expect(entry.kind == ProjectStructureEntryKind::Survey
                    && entry.name == QStringLiteral("from-config")
                    && normalizedPathForComparison(entry.sourceFile) == normalizedPathForComparison(sourceFile),
                "The thconfig source graph scan did not resolve the source target.")) {
        return 1;
    }

    const ProjectIndexSnapshot nestedSnapshot = ProjectStructureIndex::scanProjectIndex(
        projectDir.filePath(QStringLiteral("nested")),
        &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(nestedSnapshot.entries.size() == 1,
                "Opening a nested directory as the project root should use its root-level config.")) {
        return 1;
    }

    const ProjectStructureEntry &nestedEntry = nestedSnapshot.entries.first();
    if (!expect(nestedEntry.kind == ProjectStructureEntryKind::Survey
                    && nestedEntry.name == QStringLiteral("nested-config")
                    && normalizedPathForComparison(nestedEntry.sourceFile) == normalizedPathForComparison(nestedSourceFile),
                "The nested project root did not resolve its own thconfig source graph.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexRootConfigDisambiguationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("alpha.thconfig")),
                              QStringLiteral("source alpha.th\n")),
                "The first root thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("beta.thconfig")),
                              QStringLiteral("source beta.th\n")),
                "The second root thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("alpha.th")),
                              QStringLiteral(
                                  "survey alpha\n"
                                  "endsurvey alpha\n")),
                "The alpha source file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("beta.th")),
                              QStringLiteral(
                                  "survey beta\n"
                                  "endsurvey beta\n")),
                "The beta source file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot ambiguousSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                           &errorMessage);
    if (!expect(!errorMessage.isEmpty(),
                "Multiple root .thconfig files should require an explicit project target config.")) {
        return 1;
    }
    if (!expect(ambiguousSnapshot.entries.isEmpty(),
                "The project index should not silently merge multiple root .thconfig graphs.")) {
        return 1;
    }

    const ProjectIndexSnapshot betaSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                      QHash<QString, QString>(),
                                                                                      QStringLiteral("beta.thconfig"),
                                                                                      &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(betaSnapshot.entries.size() == 1,
                "The preferred project target config should select one root graph.")) {
        return 1;
    }
    if (!expect(betaSnapshot.entries.first().kind == ProjectStructureEntryKind::Survey
                    && betaSnapshot.entries.first().name == QStringLiteral("beta"),
                "The preferred project target config did not select the expected graph.")) {
        return 1;
    }

    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("thconfig")),
                              QStringLiteral("source alpha.th\n")),
                "The default root thconfig file could not be written.")) {
        return 1;
    }

    const ProjectIndexSnapshot defaultSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                         &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(defaultSnapshot.entries.size() == 1
                    && defaultSnapshot.entries.first().name == QStringLiteral("alpha"),
                "A root thconfig file should be the default project graph when no preferred config is set.")) {
        return 1;
    }

    return 0;
}

int runTh2ObjectIndexGroupingTest()
{
    const QVector<ProjectStructureEntry> entries = ProjectStructureIndex::scanTh2Objects(
        QStringLiteral("/tmp/example.th2"),
        QStringLiteral(
            "scrap s1\n"
            "point 0 0 station -name a1\n"
            "point 1 1 station -name a2\n"
            "endscrap\n"));

    if (!expect(entries.size() == 3, "The TH2 object scan should not duplicate the current scrap group.")) {
        return 1;
    }
    if (!expect(entries.at(0).kind == ProjectStructureEntryKind::Scrap
                    && entries.at(1).kind == ProjectStructureEntryKind::Station
                    && entries.at(2).kind == ProjectStructureEntryKind::Station,
                "The TH2 object scan returned unexpected entry kinds.")) {
        return 1;
    }
    if (!expect(!entries.at(0).objectId.isEmpty(), "The TH2 scrap object ID should not be empty.")) {
        return 1;
    }
    if (!expect(entries.at(1).parentObjectId == entries.at(0).objectId
                    && entries.at(2).parentObjectId == entries.at(0).objectId,
                "The TH2 object scan should attach object entries to the current scrap object ID.")) {
        return 1;
    }

    const QVector<ProjectStructureEntry> shiftedEntries = ProjectStructureIndex::scanTh2Objects(
        QStringLiteral("/tmp/example.th2"),
        QStringLiteral(
            "\n"
            "scrap s1\n"
            "point 0 0 station -name a1\n"
            "point 1 1 station -name a2\n"
            "endscrap\n"));

    if (!expect(shiftedEntries.size() == entries.size(), "The shifted TH2 object scan returned an unexpected entry count.")) {
        return 1;
    }
    if (!expect(shiftedEntries.at(1).objectId == entries.at(1).objectId,
                "The TH2 object ID should stay stable when source line numbers shift.")) {
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
    if (runProjectIndexThconfigSourceGraphTest() != 0) {
        return 1;
    }
    if (runProjectIndexRootConfigDisambiguationTest() != 0) {
        return 1;
    }
    if (runTh2ObjectIndexGroupingTest() != 0) {
        return 1;
    }

    return 0;
}
