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
                                  "  map child-map.m\n"
                                  "    s1\n"
                                  "  endmap\n"
                                  "  map cave-map.m\n"
                                  "    stale-scrap.s\n"
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
                                "  map child-map.m\n"
                                "    s1@branch.cave\n"
                                "    preview above cave-map.m\n"
                                "  endmap\n"
                                "  map cave-map.m\n"
                                "    child-map.m\n"
                                "    s1@branch.cave\n"
                                "    break\n"
                                "    missing-scrap.s\n"
                                "    missing-map.m\n"
                                "  endmap\n"
                                "endsurvey cave\n"));

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                  inMemoryContents,
                                                                                  &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(snapshot.rootConfigPath.isEmpty(),
                "The project index should leave root config empty when no thconfig graph is used.")) {
        return 1;
    }
    if (!expect(snapshot.rootFilePaths.size() == 1
                    && normalizedPathForComparison(snapshot.rootFilePaths.first()) == normalizedPathForComparison(rootFile),
                "The project index should expose the inferred root source file.")) {
        return 1;
    }

    auto findEntry = [](const QVector<ProjectStructureEntry> &entries,
                        ProjectStructureEntryKind kind,
                        const QString &name,
                        ProjectStructureEntry *foundEntry) {
        for (const ProjectStructureEntry &entry : entries) {
            if (entry.kind == kind && entry.name == name) {
                if (foundEntry != nullptr) {
                    *foundEntry = entry;
                }
                return true;
            }
        }

        return false;
    };

    ProjectStructureEntry mapEntry;
    ProjectStructureEntry childMapEntry;
    ProjectStructureEntry scrapEntry;
    const bool foundMap = findEntry(snapshot.entries,
                                    ProjectStructureEntryKind::Map,
                                    QStringLiteral("cave-map.m"),
                                    &mapEntry);
    const bool foundChildMap = findEntry(snapshot.entries,
                                         ProjectStructureEntryKind::Map,
                                         QStringLiteral("child-map.m"),
                                         &childMapEntry);
    const bool foundScrap = findEntry(snapshot.entries,
                                      ProjectStructureEntryKind::Scrap,
                                      QStringLiteral("s1"),
                                      &scrapEntry);
    if (!expect(foundMap, "The project index did not find the map entry.")) {
        return 1;
    }
    if (!expect(foundChildMap, "The project index did not find the child map entry.")) {
        return 1;
    }
    if (!expect(foundScrap, "The project index did not find the scrap entry.")) {
        return 1;
    }
    if (!expect(!mapEntry.objectId.isEmpty(), "The project index map entry should expose a stable object ID.")) {
        return 1;
    }

    const QString mapKey = ProjectStructureIndex::structureEntryNodeKey(mapEntry);
    if (!expect(mapKey == mapEntry.objectId, "The structure node key should prefer the stable object ID.")) {
        return 1;
    }
    const QString childMapKey = ProjectStructureIndex::structureEntryNodeKey(childMapEntry);
    const QSet<QString> childMapReferences = snapshot.mapChildReferencesByMapKey.value(mapKey);
    if (!expect(childMapReferences.contains(childMapKey),
                "The project index did not resolve the map-to-map composition reference.")) {
        return 1;
    }
    if (!expect(snapshot.mapChildReferencesByMapKey.value(childMapKey).isEmpty(),
                "Preview references should not be treated as map composition hierarchy.")) {
        return 1;
    }

    const QString scrapKey = ProjectStructureIndex::structureEntryNodeKey(scrapEntry);
    const QSet<QString> scrapReferences = snapshot.mapScrapReferencesByMapKey.value(mapKey);
    if (!expect(scrapReferences.contains(scrapKey),
                "The project index did not resolve the namespaced map-to-scrap reference from in-memory source text.")) {
        return 1;
    }
    for (const ProjectIndexDiagnostic &diagnostic : snapshot.diagnostics) {
        if (!expect(diagnostic.referencedName != QStringLiteral("stale-scrap.s"),
                    "The project index used stale on-disk source text for map references.")) {
            return 1;
        }
    }
    if (!expect(snapshot.diagnostics.size() == 2,
                "The project index should report unresolved map scrap and map references.")) {
        return 1;
    }
    const ProjectIndexDiagnostic &scrapDiagnostic = snapshot.diagnostics.at(0);
    if (!expect(scrapDiagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapScrapReference,
                "The project index reported an unexpected unresolved scrap diagnostic kind.")) {
        return 1;
    }
    if (!expect(scrapDiagnostic.sourceObjectId == mapEntry.objectId,
                "The unresolved map scrap reference diagnostic should point at the owning map object.")) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(scrapDiagnostic.sourceFile) == normalizedPathForComparison(rootFile),
                "The unresolved map scrap reference diagnostic source file is incorrect.")) {
        return 1;
    }
    if (!expect(scrapDiagnostic.lineNumber == 11,
                "The unresolved map scrap reference diagnostic line number is incorrect.")) {
        return 1;
    }
    if (!expect(scrapDiagnostic.referencedName == QStringLiteral("missing-scrap.s"),
                "The unresolved map scrap reference diagnostic target name is incorrect.")) {
        return 1;
    }
    const ProjectIndexDiagnostic &mapDiagnostic = snapshot.diagnostics.at(1);
    if (!expect(mapDiagnostic.kind == ProjectIndexDiagnosticKind::UnknownMapReference,
                "The project index reported an unexpected unresolved map diagnostic kind.")) {
        return 1;
    }
    if (!expect(mapDiagnostic.sourceObjectId == mapEntry.objectId
                    && mapDiagnostic.lineNumber == 12
                    && mapDiagnostic.referencedName == QStringLiteral("missing-map.m"),
                "The unresolved map reference diagnostic payload is incorrect.")) {
        return 1;
    }

    inMemoryContents.insert(normalizedPathForComparison(rootFile),
                            QStringLiteral(
                                "\n"
                                "survey cave\n"
                                "  input maps/map.th2\n"
                                "  map child-map.m\n"
                                "    s1@branch.cave\n"
                                "    preview above cave-map.m\n"
                                "  endmap\n"
                                "  map cave-map.m\n"
                                "    child-map.m\n"
                                "    s1@branch.cave\n"
                                "    break\n"
                                "    missing-scrap.s\n"
                                "    missing-map.m\n"
                                "  endmap\n"
                                "endsurvey cave\n"));
    const ProjectIndexSnapshot shiftedSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                         inMemoryContents,
                                                                                         &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }

    ProjectStructureEntry shiftedMapEntry;
    const bool foundShiftedMap = findEntry(shiftedSnapshot.entries,
                                           ProjectStructureEntryKind::Map,
                                           QStringLiteral("cave-map.m"),
                                           &shiftedMapEntry);
    if (!expect(foundShiftedMap, "The shifted project index did not find the map entry.")) {
        return 1;
    }
    if (!expect(shiftedMapEntry.objectId == mapEntry.objectId,
                "The project index object ID should stay stable when source line numbers shift.")) {
        return 1;
    }
    if (!expect(shiftedSnapshot.diagnostics.size() == 2
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
    if (!expect(normalizedPathForComparison(snapshot.projectRootPath) == normalizedPathForComparison(projectDir.path()),
                "The thconfig source graph scan should expose the normalized project root path.")) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(snapshot.rootConfigPath) == normalizedPathForComparison(configFile),
                "The thconfig source graph scan should expose the resolved root config path.")) {
        return 1;
    }
    if (!expect(snapshot.rootFilePaths.size() == 1
                    && normalizedPathForComparison(snapshot.rootFilePaths.first()) == normalizedPathForComparison(configFile),
                "The thconfig source graph scan should expose the config as the root traversal file.")) {
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
    if (!expect(normalizedPathForComparison(nestedSnapshot.rootConfigPath) == normalizedPathForComparison(nestedConfigFile),
                "Opening a nested directory as the project root should expose its own root config path.")) {
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
    if (!expect(ambiguousSnapshot.rootConfigPath.isEmpty() && ambiguousSnapshot.rootFilePaths.isEmpty(),
                "An ambiguous project index should not expose a resolved root config or traversal root.")) {
        return 1;
    }

    const ProjectIndexSnapshot betaSnapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(),
                                                                                      QHash<QString, QString>(),
                                                                                      QStringLiteral("beta.thconfig"),
                                                                                      &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(betaSnapshot.rootConfigPath)
                    == normalizedPathForComparison(projectDir.filePath(QStringLiteral("beta.thconfig"))),
                "The preferred project target config should be exposed as the resolved root config.")) {
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
    if (!expect(normalizedPathForComparison(defaultSnapshot.rootConfigPath)
                    == normalizedPathForComparison(projectDir.filePath(QStringLiteral("thconfig"))),
                "A root thconfig file should be exposed as the resolved default root config.")) {
        return 1;
    }
    if (!expect(defaultSnapshot.entries.size() == 1
                    && defaultSnapshot.entries.first().name == QStringLiteral("alpha"),
                "A root thconfig file should be the default project graph when no preferred config is set.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexThconfigWorkRootTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "The temporary project directory could not be created.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    const QString configFile = projectDir.filePath(QStringLiteral("thconfig.work"));
    const QString sourceFile = projectDir.filePath(QStringLiteral("work.th"));
    if (!expect(writeTextFile(configFile,
                              QStringLiteral("source work.th\n")),
                "The thconfig.work file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(sourceFile,
                              QStringLiteral(
                                  "survey work\n"
                                  "endsurvey work\n")),
                "The thconfig.work source file could not be written.")) {
        return 1;
    }

    QString errorMessage;
    const ProjectIndexSnapshot snapshot = ProjectStructureIndex::scanProjectIndex(projectDir.path(), &errorMessage);
    if (!expect(errorMessage.isEmpty(), errorMessage.toUtf8().constData())) {
        return 1;
    }
    if (!expect(normalizedPathForComparison(snapshot.rootConfigPath) == normalizedPathForComparison(configFile),
                "A single thconfig.* file should be accepted as the project root config.")) {
        return 1;
    }
    if (!expect(snapshot.entries.size() == 1
                    && snapshot.entries.first().kind == ProjectStructureEntryKind::Survey
                    && snapshot.entries.first().name == QStringLiteral("work"),
                "The thconfig.work root config did not select the expected source graph.")) {
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
    if (runProjectIndexThconfigWorkRootTest() != 0) {
        return 1;
    }
    if (runTh2ObjectIndexGroupingTest() != 0) {
        return 1;
    }

    return 0;
}
