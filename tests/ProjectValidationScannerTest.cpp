#include "../src/app/ProjectValidationScanner.h"
#include "../src/app/text_editor/TextEditorValidationCatalog.h"
#include "../src/core/CommandCatalogStore.h"
#include "../src/core/DocumentFile.h"
#include "../src/core/TherionCommandSyntax.h"
#include "../src/core/TherionSourceReferenceResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

using namespace TherionStudio;

namespace
{
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
    const QByteArray bytes = contents.toUtf8();
    return file.write(bytes) == bytes.size();
}

QString canonicalOrAbsolutePath(const QString &path)
{
    return canonicalOrAbsoluteFilePath(path);
}

TherionSourceValidationCatalog testCatalog()
{
    TherionSourceValidationCatalog catalog;
    catalog.commandNames = {
        QStringLiteral("scrap"),
        QStringLiteral("line"),
        QStringLiteral("point"),
    };
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("scrap"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("line"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("point"), 3);
    catalog.commandOptionNames.insert(QStringLiteral("line"), {QStringLiteral("-close"), QStringLiteral("-clip")});
    catalog.commandOptionNames.insert(QStringLiteral("point"), {QStringLiteral("-text")});
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-close")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-clip")),
                                                 QStringLiteral("EXACTLY_ONE"));
    return catalog;
}

TherionSourceValidationCatalog contextualDocumentTypeCatalog()
{
    TherionSourceValidationCatalog catalog = testCatalog();
    catalog.commandNames.unite({
        QStringLiteral("survey"),
        QStringLiteral("centerline"),
        QStringLiteral("data"),
        QStringLiteral("cs"),
        QStringLiteral("map"),
        QStringLiteral("break"),
        QStringLiteral("join"),
        QStringLiteral("source"),
        QStringLiteral("input"),
        QStringLiteral("select"),
        QStringLiteral("layout"),
        QStringLiteral("export"),
        QStringLiteral("area"),
    });

    catalog.commandContexts.insert(QStringLiteral("survey"), {QStringLiteral("none"), QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("centerline"), {QStringLiteral("none"), QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("data"), {QStringLiteral("centerline")});
    catalog.commandContexts.insert(QStringLiteral("cs"), {QStringLiteral("centerline"), QStringLiteral("layout"), QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("map"), {QStringLiteral("none"), QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("break"), {QStringLiteral("centerline"), QStringLiteral("map")});
    catalog.commandContexts.insert(QStringLiteral("join"), {QStringLiteral("none"), QStringLiteral("survey"), QStringLiteral("scrap")});
    catalog.commandContexts.insert(QStringLiteral("source"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("input"), {QStringLiteral("all")});
    catalog.commandContexts.insert(QStringLiteral("select"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("layout"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("export"), {QStringLiteral("none")});
    catalog.commandContexts.insert(QStringLiteral("scrap"), {QStringLiteral("none"), QStringLiteral("survey")});
    catalog.commandContexts.insert(QStringLiteral("point"), {QStringLiteral("scrap")});
    catalog.commandContexts.insert(QStringLiteral("line"), {QStringLiteral("scrap")});
    catalog.commandContexts.insert(QStringLiteral("area"), {QStringLiteral("scrap")});

    catalog.commandDocumentTypes.insert(QStringLiteral("survey"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("centerline"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("data"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("cs"), {QStringLiteral("th"), QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("map"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("break"), {QStringLiteral("th")});
    catalog.commandDocumentTypes.insert(QStringLiteral("join"), {QStringLiteral("th"), QStringLiteral("th2")});
    catalog.commandDocumentTypes.insert(QStringLiteral("source"), {QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("input"), {QStringLiteral("th"), QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("select"), {QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("layout"), {QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("export"), {QStringLiteral("thconfig")});
    catalog.commandDocumentTypes.insert(QStringLiteral("scrap"), {QStringLiteral("th2")});
    catalog.commandDocumentTypes.insert(QStringLiteral("point"), {QStringLiteral("th2")});
    catalog.commandDocumentTypes.insert(QStringLiteral("line"), {QStringLiteral("th2")});
    catalog.commandDocumentTypes.insert(QStringLiteral("area"), {QStringLiteral("th2")});
    return catalog;
}

TherionSourceValidationCatalog appValidationCatalog()
{
    const QString catalogPath =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../resources/therion_command_catalog.json"));
    return validationCatalogFromCommandCatalog(CommandCatalogStore::fromFile(catalogPath).catalogObject());
}

struct ValidationWaitResult
{
    bool received = false;
    ProjectValidationScanner::Result result;
};

ValidationWaitResult waitForValidation(ProjectValidationScanner &scanner)
{
    ValidationWaitResult waitResult;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);

    QObject::connect(&scanner,
                     &ProjectValidationScanner::validationFinished,
                     &loop,
                     [&](const ProjectValidationScanner::Result &result) {
                         waitResult.received = true;
                         waitResult.result = result;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeout.start();
    loop.exec();
    return waitResult;
}

bool containsFinding(const ProjectValidationScanner::Result &result,
                     const QString &filePath,
                     const QString &code)
{
    const QString normalizedPath = canonicalOrAbsolutePath(filePath);
    for (const ProjectValidationScanner::Finding &finding : result.findings) {
        if (canonicalOrAbsolutePath(finding.filePath) == normalizedPath
            && finding.diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

int findingCount(const ProjectValidationScanner::Result &result,
                 const QString &filePath,
                 const QString &code)
{
    const QString normalizedPath = canonicalOrAbsolutePath(filePath);
    int count = 0;
    for (const ProjectValidationScanner::Finding &finding : result.findings) {
        if (canonicalOrAbsolutePath(finding.filePath) == normalizedPath
            && finding.diagnostic.code == code) {
            ++count;
        }
    }
    return count;
}

bool findingHasSeverity(const ProjectValidationScanner::Result &result,
                        const QString &filePath,
                        const QString &code,
                        TherionSourceDiagnosticSeverity severity)
{
    const QString normalizedPath = canonicalOrAbsolutePath(filePath);
    for (const ProjectValidationScanner::Finding &finding : result.findings) {
        if (canonicalOrAbsolutePath(finding.filePath) == normalizedPath
            && finding.diagnostic.code == code
            && finding.diagnostic.severity == severity) {
            return true;
        }
    }
    return false;
}

const TherionSourceDiagnostic *findingDiagnostic(const ProjectValidationScanner::Result &result,
                                                const QString &filePath,
                                                const QString &code)
{
    const QString normalizedPath = canonicalOrAbsolutePath(filePath);
    for (const ProjectValidationScanner::Finding &finding : result.findings) {
        if (canonicalOrAbsolutePath(finding.filePath) == normalizedPath
            && finding.diagnostic.code == code) {
            return &finding.diagnostic;
        }
    }
    return nullptr;
}

int runFilesystemValidationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString mapFile = QDir(tempDir.path()).filePath(QStringLiteral("map.th2"));
    const QString configFile = QDir(tempDir.path()).filePath(QStringLiteral("thconfig"));
    const QString ignoredFile = QDir(tempDir.path()).filePath(QStringLiteral("notes.md"));
    if (!expect(writeTextFile(mapFile,
                              QStringLiteral("scrap test\n"
                                             "line wall -clip off \"-clip off\"\n"
                                             "endline\n"
                                             "endscrap\n")),
                "Temporary .th2 file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(configFile,
                              QStringLiteral("custom-config-command value\n"
                                             "line wall -clip off \"-clip off\"\n")),
                "Temporary thconfig file could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(ignoredFile,
                              QStringLiteral("line wall -clip off \"-clip off\"\n")),
                "Temporary ignored file could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), testCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "ProjectValidationScanner did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Filesystem validation should not report an error.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result, mapFile, QStringLiteral("malformed-option-token")),
                "Project validation should report malformed Therion lines.")) {
        return 1;
    }
    if (!expect(!containsFinding(waitResult.result, configFile, QStringLiteral("unknown-command")),
                "Project validation should suppress uncertain unknown-command warnings for thconfig files.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result, configFile, QStringLiteral("malformed-option-token")),
                "Project validation should keep safe thconfig line cleanup diagnostics.")) {
        return 1;
    }
    if (!expect(!containsFinding(waitResult.result, ignoredFile, QStringLiteral("malformed-option-token")),
                "Project validation should ignore non-Therion files.")) {
        return 1;
    }

    return 0;
}

int runDashPrefixedTextValidationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString mapFile = QDir(tempDir.path()).filePath(QStringLiteral("labels.th2"));
    if (!expect(writeTextFile(mapFile,
                              QStringLiteral("scrap test\n"
                                             "point 4505.0 -1446.0 label -text \"-21 m\"\n"
                                             "endscrap\n")),
                "Temporary .th2 label file could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), testCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Dash-prefixed text validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Dash-prefixed text validation should not report an error.")) {
        return 1;
    }
    if (!expect(!containsFinding(waitResult.result, mapFile, QStringLiteral("unknown-option")),
                "Project validation should keep dash-prefixed point text values as text, not unknown options.")) {
        return 1;
    }

    return 0;
}

int runInMemoryValidationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("live.th2"));
    if (!expect(writeTextFile(filePath,
                              QStringLiteral("scrap stale\n"
                                             "endscrap\n")),
                "Temporary stale file could not be written.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(canonicalOrAbsolutePath(filePath),
                            QStringLiteral("scrap live\n"
                                           "line wall -clip off \"-clip off\"\n"
                                           "endline\n"
                                           "endscrap\n"));

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), testCatalog(), inMemoryContents);

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "In-memory validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "In-memory validation should not report an error.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result, filePath, QStringLiteral("malformed-option-token")),
                "Project validation should use unsaved in-memory document text.")) {
        return 1;
    }

    return 0;
}

int runMissingSourceReferenceValidationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString sourceFile = QDir(tempDir.path()).filePath(QStringLiteral("index.th"));
    const QString existingInputFile = QDir(tempDir.path()).filePath(QStringLiteral("existing.th"));
    const QString configFile = QDir(tempDir.path()).filePath(QStringLiteral("thconfig"));
    const QString inMemoryOnlyFile = QDir(tempDir.path()).filePath(QStringLiteral("generated.th"));
    if (!expect(writeTextFile(sourceFile,
                              QStringLiteral("survey cave\n"
                                             "  input existing\n"
                                             "  input missing\n"
                                             "  input missing_explicit.th222\n"
                                             "  input generated\n"
                                             "endsurvey\n")),
                "Temporary source reference fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(existingInputFile,
                              QStringLiteral("survey existing\n"
                                             "endsurvey\n")),
                "Temporary existing input fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(configFile,
                              QStringLiteral("source index.th\n"
                                             "source absent.th\n")),
                "Temporary config reference fixture could not be written.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(canonicalOrAbsolutePath(inMemoryOnlyFile),
                            QStringLiteral("survey generated\n"
                                           "endsurvey\n"));
    QSet<QString> knownReferencePaths;
    knownReferencePaths.insert(canonicalOrAbsolutePath(inMemoryOnlyFile));
    if (!expect(!resolveTherionSourceReferencePath(sourceFile,
                                                  QStringLiteral("generated"),
                                                  knownReferencePaths).isEmpty(),
                "Source-reference resolver should resolve implicit .th paths from in-memory project files.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), inMemoryContents);

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Missing source reference validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Missing source reference validation should not report an error.")) {
        return 1;
    }
    const int sourceMissingReferenceCount =
        findingCount(waitResult.result, sourceFile, QStringLiteral("missing-source-reference"));
    if (!expect(sourceMissingReferenceCount == 2,
                "Project validation should report only unresolved input/source references in .th files.")) {
        std::cerr << "Actual missing-source-reference count for .th file: "
                  << sourceMissingReferenceCount << '\n';
        for (const ProjectValidationScanner::Finding &finding : waitResult.result.findings) {
            if (canonicalOrAbsolutePath(finding.filePath) == canonicalOrAbsolutePath(sourceFile)
                && finding.diagnostic.code == QStringLiteral("missing-source-reference")) {
                std::cerr << "  " << finding.diagnostic.message.toStdString() << '\n';
            }
        }
        return 1;
    }
    if (!expect(findingCount(waitResult.result, configFile, QStringLiteral("missing-source-reference")) == 1,
                "Project validation should report unresolved source references in thconfig files.")) {
        return 1;
    }

    return 0;
}

int runUnsavedMissingSourceReferenceValidationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString sourceFile = QDir(tempDir.path()).filePath(QStringLiteral("index.th"));
    const QString existingInputFile = QDir(tempDir.path()).filePath(QStringLiteral("existing.th"));
    if (!expect(writeTextFile(sourceFile,
                              QStringLiteral("input existing\n")),
                "Temporary valid source reference fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(existingInputFile,
                              QStringLiteral("survey existing\n"
                                             "endsurvey\n")),
                "Temporary existing source target fixture could not be written.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(QFileInfo(sourceFile).absoluteFilePath(),
                            QStringLiteral("input missing_from_unsaved_editor\n"));

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), inMemoryContents);

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Unsaved missing source reference validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Unsaved missing source reference validation should not report an error.")) {
        return 1;
    }
    if (!expect(findingCount(waitResult.result, sourceFile, QStringLiteral("missing-source-reference")) == 1,
                "Project validation should use unsaved editor text when checking input/source references.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexDiagnosticProjectionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("maps/a"))
                    && projectDir.mkpath(QStringLiteral("maps/b")),
                "Temporary map directories could not be created.")) {
        return 1;
    }

    const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
    const QString mapAFile = projectDir.filePath(QStringLiteral("maps/a/map.th2"));
    const QString mapBFile = projectDir.filePath(QStringLiteral("maps/b/map.th2"));

    if (!expect(writeTextFile(rootFile,
                              QStringLiteral("survey cave\n"
                                             "  input maps/a/map.th2\n"
                                             "  input maps/b/map.th2\n"
                                             "  join missing-line.s\n"
                                             "  centerline\n"
                                             "    data normal from to compass clino tape\n"
                                             "    known other 0 0 1\n"
                                             "    equate known@cave missing@cave\n"
                                             "  endcenterline\n"
                                             "  map branch-map.m\n"
                                             "  endmap\n"
                                             "  map branch-map.m\n"
                                             "  endmap\n"
                                             "  map root-map.m\n"
                                             "    target.s\n"
                                             "    branch-map.m\n"
                                             "    missing-map.m\n"
                                             "  endmap\n"
                                             "endsurvey cave\n")),
                "Project-index diagnostic root fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapAFile,
                              QStringLiteral("scrap target.s\n"
                                             "endscrap\n")),
                "Project-index diagnostic first map fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapBFile,
                              QStringLiteral("scrap target.s\n"
                                             "endscrap\n")),
                "Project-index diagnostic second map fixture could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Project-index diagnostic projection did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Project-index diagnostic projection should not report an error.")) {
        return 1;
    }
    if (!expect(findingHasSeverity(waitResult.result,
                                   rootFile,
                                   QStringLiteral("ambiguous-map-scrap-reference"),
                                   TherionSourceDiagnosticSeverity::Error),
                "Project validation should expose ambiguous map scrap references from the project index.")) {
        return 1;
    }
    if (!expect(findingHasSeverity(waitResult.result,
                                   rootFile,
                                   QStringLiteral("ambiguous-map-reference"),
                                   TherionSourceDiagnosticSeverity::Error),
                "Project validation should expose ambiguous map references from the project index.")) {
        return 1;
    }
    if (!expect(findingHasSeverity(waitResult.result,
                                   rootFile,
                                   QStringLiteral("unknown-map-reference"),
                                   TherionSourceDiagnosticSeverity::Error),
                "Project validation should expose unresolved map references from the project index.")) {
        return 1;
    }
    if (!expect(findingHasSeverity(waitResult.result,
                                   rootFile,
                                   QStringLiteral("unknown-join-reference"),
                                   TherionSourceDiagnosticSeverity::Error),
                "Project validation should expose unresolved join references from the project index.")) {
        return 1;
    }
    if (!expect(findingHasSeverity(waitResult.result,
                                   rootFile,
                                   QStringLiteral("unknown-station-reference"),
                                   TherionSourceDiagnosticSeverity::Error),
                "Project validation should expose unresolved station references from the project index.")) {
        return 1;
    }
    const TherionSourceDiagnostic *unknownMapDiagnostic =
        findingDiagnostic(waitResult.result, rootFile, QStringLiteral("unknown-map-reference"));
    if (!expect(unknownMapDiagnostic != nullptr,
                "Unknown map reference diagnostic should be available for range checks.")) {
        return 1;
    }
    if (!expect(unknownMapDiagnostic->columnNumber == 5
                    && unknownMapDiagnostic->columnLength == QStringLiteral("missing-map.m").size(),
                "Unknown map reference diagnostic should cover the full referenced token.")) {
        return 1;
    }
    if (!expect(findingHasSeverity(waitResult.result,
                                   rootFile,
                                   QStringLiteral("mixed-map-and-scrap-references"),
                                   TherionSourceDiagnosticSeverity::Warning),
                "Project validation should expose mixed map/scrap composition as a warning.")) {
        return 1;
    }

    return 0;
}

int runDuplicateObjectIdDiagnosticIsNotDuplicatedTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    const QString rootFile = projectDir.filePath(QStringLiteral("root.th"));
    const QString mapFile = projectDir.filePath(QStringLiteral("map.th2"));

    if (!expect(writeTextFile(rootFile,
                              QStringLiteral("survey cave\n"
                                             "  input map.th2\n"
                                             "endsurvey\n")),
                "Duplicate object-id root fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapFile,
                              QStringLiteral("scrap test\n"
                                             "line wall -id line-1\n"
                                             "endline\n"
                                             "line border -id line-1\n"
                                             "endline\n"
                                             "endscrap\n")),
                "Duplicate object-id map fixture could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Duplicate object-id project validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Duplicate object-id project validation should not report a scanner error.")) {
        return 1;
    }
    if (!expect(findingCount(waitResult.result, mapFile, QStringLiteral("duplicate-object-id")) == 1,
                "Project validation should not duplicate equivalent duplicate-object-id diagnostics.")) {
        return 1;
    }

    return 0;
}

int runRootConfigSourceGraphValidationTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(projectDir.mkpath(QStringLiteral("01_zadni_pole")),
                "Root-config source graph directory could not be created.")) {
        return 1;
    }

    const QString configFile = projectDir.filePath(QStringLiteral("babice.thconfig"));
    const QString rootFile = projectDir.filePath(QStringLiteral("babice.th"));
    const QString branchFile = projectDir.filePath(QStringLiteral("01_zadni_pole/zadni_pole.th"));
    if (!expect(writeTextFile(configFile,
                              QStringLiteral("source babice.th\n")),
                "Root-config source graph config fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(rootFile,
                              QStringLiteral("survey babicka_plosina\n"
                                             "  input povrchova_mereni/totalka.th222\n"
                                             "  input 01_zadni_pole/zadni_pole.th\n"
                                             "  map babicka_plosina.m\n"
                                             "    zadni_pole.m@zadni_polesss\n"
                                             "  endmap\n"
                                             "endsurvey\n")),
                "Root-config source graph root fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(branchFile,
                              QStringLiteral("survey zadni_pole\n"
                                             "  map zadni_pole.m\n"
                                             "  endmap\n"
                                             "endsurvey\n")),
                "Root-config source graph branch fixture could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Root-config source graph validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Root-config source graph validation should not report an error.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result, rootFile, QStringLiteral("missing-source-reference")),
                "Project validation should report missing input references in the root config source graph.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result, rootFile, QStringLiteral("unknown-map-reference")),
                "Project validation should report unresolved map references in the root config source graph.")) {
        return 1;
    }

    return 0;
}

int runProjectIndexUnavailableDiagnosticTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    QDir projectDir(tempDir.path());
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("alpha.thconfig")),
                              QStringLiteral("source alpha.th\n")),
                "First ambiguous thconfig fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("beta.thconfig")),
                              QStringLiteral("source beta.th\n")),
                "Second ambiguous thconfig fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("alpha.th")),
                              QStringLiteral("survey alpha\n"
                                             "endsurvey\n")),
                "First ambiguous source fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(projectDir.filePath(QStringLiteral("beta.th")),
                              QStringLiteral("survey beta\n"
                                             "endsurvey\n")),
                "Second ambiguous source fixture could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Project-index unavailable validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Project-index unavailable validation should not report a scanner error.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result,
                                tempDir.path(),
                                QStringLiteral("project-index-unavailable")),
                "Project validation should expose project-index root graph errors instead of silently dropping them.")) {
        return 1;
    }

    return 0;
}

int runDocumentTypeContextProjectionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString sourceFile = QDir(tempDir.path()).filePath(QStringLiteral("index.th"));
    const QString configFile = QDir(tempDir.path()).filePath(QStringLiteral("thconfig"));
    const QString mapFile = QDir(tempDir.path()).filePath(QStringLiteral("scraps.th2"));

    if (!expect(writeTextFile(sourceFile,
                              QStringLiteral("survey cave\n"
                                             "  centerline\n"
                                             "    cs long-lat\n"
                                             "    data normal from to compass clino tape\n"
                                             "    1 2 0 0 1\n"
                                             "  endcenterline\n"
                                             "  map cave.m\n"
                                             "    scrap1\n"
                                             "    break\n"
                                             "    scrap2\n"
                                             "  endmap\n"
                                             "  join scrap1 scrap2\n"
                                             "endsurvey\n")),
                "Temporary .th context fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(configFile,
                              QStringLiteral("source index.th\n"
                                             "input ../layouts\n"
                                             "cs iJTSK\n"
                                             "select cave.m@cave\n"
                                             "layout l_plan\n"
                                             "  cs iJTSK\n"
                                             "endlayout\n"
                                             "export map -output out.pdf -layout l_plan\n")),
                "Temporary thconfig context fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(mapFile,
                              QStringLiteral("scrap s1 -projection plan\n"
                                             "point 0 0 station -name 1@survey\n"
                                             "line wall\n"
                                             "  0 0\n"
                                             "  1 1\n"
                                             "endline\n"
                                             "area water\n"
                                             "  border1\n"
                                             "endarea\n"
                                             "endscrap\n")),
                "Temporary .th2 context fixture could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Context/document-type validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Context/document-type validation should not report an error.")) {
        return 1;
    }
    if (!expect(!containsFinding(waitResult.result, sourceFile, QStringLiteral("invalid-command-context"))
                && !containsFinding(waitResult.result, configFile, QStringLiteral("invalid-command-context"))
                && !containsFinding(waitResult.result, mapFile, QStringLiteral("invalid-command-context")),
                "Project validation should not report invalid contexts for representative .th, thconfig, and .th2 commands.")) {
        return 1;
    }
    if (!expect(!containsFinding(waitResult.result, sourceFile, QStringLiteral("invalid-document-type"))
                && !containsFinding(waitResult.result, configFile, QStringLiteral("invalid-document-type"))
                && !containsFinding(waitResult.result, mapFile, QStringLiteral("invalid-document-type")),
                "Project validation should not report invalid document types for representative .th, thconfig, and .th2 commands.")) {
        return 1;
    }

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(canonicalOrAbsolutePath(configFile),
                            QStringLiteral("survey wrong\n"
                                           "endsurvey\n"));
    scanner.requestScan(tempDir.path(), contextualDocumentTypeCatalog(), inMemoryContents);
    const ValidationWaitResult invalidWaitResult = waitForValidation(scanner);
    if (!expect(invalidWaitResult.received, "In-memory document-type validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(containsFinding(invalidWaitResult.result, configFile, QStringLiteral("invalid-document-type")),
                "Project validation should apply document-type diagnostics to unsaved in-memory thconfig text.")) {
        return 1;
    }

    return 0;
}

int runSupersededScanStillDeliversLatestResultTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("live.th2"));
    if (!expect(writeTextFile(filePath,
                              QStringLiteral("scrap stale\n"
                                             "endscrap\n")),
                "Temporary superseded-scan file could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);

    bool requestedSupersedingScan = false;
    bool receivedFirstResult = false;
    bool receivedLatestResult = false;
    ProjectValidationScanner::Result latestResult;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);
    QObject::connect(&scanner,
                     &ProjectValidationScanner::validationStarted,
                     &scanner,
                     [&](quint64 generation, const QString &) {
                         if (generation != 1 || requestedSupersedingScan) {
                             return;
                         }
                         requestedSupersedingScan = true;
                         QHash<QString, QString> inMemoryContents;
                         inMemoryContents.insert(canonicalOrAbsolutePath(filePath),
                                                 QStringLiteral("scrap live\n"
                                                                "line wall -clip off \"-clip off\"\n"
                                                                "endline\n"
                                                                "endscrap\n"));
                         scanner.requestScan(tempDir.path(), testCatalog(), inMemoryContents);
                     });
    QObject::connect(&scanner,
                     &ProjectValidationScanner::validationFinished,
                     &loop,
                     [&](const ProjectValidationScanner::Result &result) {
                         if (result.generation == 1) {
                             receivedFirstResult = true;
                         } else if (result.generation == 2) {
                             receivedLatestResult = true;
                             latestResult = result;
                             loop.quit();
                         }
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    scanner.requestScan(tempDir.path(), testCatalog(), {});
    timeout.start();
    loop.exec();

    if (!expect(receivedFirstResult, "Superseded validation should still emit the completed older result.")) {
        return 1;
    }
    if (!expect(requestedSupersedingScan, "Superseded validation test did not queue a newer scan.")) {
        return 1;
    }
    if (!expect(receivedLatestResult,
                "ProjectValidationScanner should emit the latest result after a superseded scan.")) {
        return 1;
    }
    if (!expect(containsFinding(latestResult, filePath, QStringLiteral("malformed-option-token")),
                "Latest project validation result should use the superseding in-memory document text.")) {
        return 1;
    }

    return 0;
}

int runRepeatedRequestsDoNotStarveDebounceTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("live.th2"));
    if (!expect(writeTextFile(filePath,
                              QStringLiteral("scrap live\n"
                                             "line wall -clip off \"-clip off\"\n"
                                             "endline\n"
                                             "endscrap\n")),
                "Temporary repeated-request file could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(120);

    QTimer requestTimer;
    requestTimer.setInterval(10);
    QObject::connect(&requestTimer, &QTimer::timeout, &scanner, [&scanner, &tempDir]() {
        scanner.requestScan(tempDir.path(), testCatalog(), {});
    });

    requestTimer.start();
    const ValidationWaitResult waitResult = waitForValidation(scanner);
    requestTimer.stop();
    if (!expect(waitResult.received, "Repeated validation requests should not starve the debounce timer.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result, filePath, QStringLiteral("malformed-option-token")),
                "Repeated validation requests should still validate the latest pending request.")) {
        return 1;
    }

    return 0;
}

int runSupersededRunningScanStartsNextImmediatelyTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("live.th2"));
    if (!expect(writeTextFile(filePath,
                              QStringLiteral("scrap stale\n"
                                             "endscrap\n")),
                "Temporary immediate-superseded-scan file could not be written.")) {
        return 1;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);

    bool requestedSupersedingScan = false;
    bool startedLatestScan = false;
    bool finishedLatestScan = false;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(1000);

    QObject::connect(&scanner,
                     &ProjectValidationScanner::validationStarted,
                     &loop,
                     [&](quint64 generation, const QString &) {
                         if (generation == 1 && !requestedSupersedingScan) {
                             requestedSupersedingScan = true;
                             scanner.setDebounceIntervalMs(10000);
                             QHash<QString, QString> inMemoryContents;
                             inMemoryContents.insert(canonicalOrAbsolutePath(filePath),
                                                     QStringLiteral("scrap live\n"
                                                                    "line wall -clip off \"-clip off\"\n"
                                                                    "endline\n"
                                                                    "endscrap\n"));
                             scanner.requestScan(tempDir.path(), testCatalog(), inMemoryContents);
                             return;
                         }
                         if (generation == 2) {
                             startedLatestScan = true;
                         }
                     });
    QObject::connect(&scanner,
                     &ProjectValidationScanner::validationFinished,
                     &loop,
                     [&](const ProjectValidationScanner::Result &result) {
                         if (result.generation == 2) {
                             finishedLatestScan = containsFinding(result, filePath, QStringLiteral("malformed-option-token"));
                             loop.quit();
                         }
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    scanner.requestScan(tempDir.path(), testCatalog(), {});
    timeout.start();
    loop.exec();

    if (!expect(requestedSupersedingScan, "Immediate superseded validation test did not queue a newer scan.")) {
        return 1;
    }
    if (!expect(startedLatestScan,
                "ProjectValidationScanner should start a superseding scan immediately after the running scan finishes.")) {
        return 1;
    }
    if (!expect(finishedLatestScan,
                "ProjectValidationScanner should emit the latest superseding validation result before waiting for debounce.")) {
        return 1;
    }

    return 0;
}

int runBabiceSampleValidationCompletesTest()
{
    const QString sampleProjectPath =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../sample_data/babice"));
    if (!QDir(sampleProjectPath).exists()) {
        return 0;
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(sampleProjectPath, contextualDocumentTypeCatalog(), {});

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Babice sample project validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Babice sample project validation should not report a scanner error.")) {
        return 1;
    }

    return 0;
}

int runBabiceSampleValidationWithOpenDocumentsTest()
{
    const QString sampleProjectPath =
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../sample_data/babice"));
    if (!QDir(sampleProjectPath).exists()) {
        return 0;
    }

    QHash<QString, QString> inMemoryContents;
    for (const QString &relativePath : {
             QStringLiteral("babice.th"),
             QStringLiteral("untitled.th2"),
             QStringLiteral("untitled2.th2"),
         }) {
        const QString filePath = QDir(sampleProjectPath).filePath(relativePath);
        QString text;
        if (!expect(DocumentFile::readTextFile(filePath, &text, nullptr, nullptr, nullptr),
                    "Babice open-document fixture could not be read.")) {
            return 1;
        }
        inMemoryContents.insert(canonicalOrAbsolutePath(filePath), text);
    }

    ProjectValidationScanner scanner;
    scanner.setDebounceIntervalMs(0);
    scanner.requestScan(sampleProjectPath, appValidationCatalog(), inMemoryContents);

    const ValidationWaitResult waitResult = waitForValidation(scanner);
    if (!expect(waitResult.received, "Babice open-document project validation did not emit validationFinished before timeout.")) {
        return 1;
    }
    if (!expect(waitResult.result.errorMessage.isEmpty(), "Babice open-document project validation should not report a scanner error.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result,
                                QDir(sampleProjectPath).filePath(QStringLiteral("untitled2.th2")),
                                QStringLiteral("malformed-option-token")),
                "Babice open-document project validation should report duplicate -clip options.")) {
        return 1;
    }
    if (!expect(containsFinding(waitResult.result,
                                QDir(sampleProjectPath).filePath(QStringLiteral("babice.th")),
                                QStringLiteral("missing-source-reference")),
                "Babice open-document project validation should report the missing input file.")) {
        return 1;
    }

    return 0;
}

}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runFilesystemValidationTest() != 0) {
        return 1;
    }
    if (runInMemoryValidationTest() != 0) {
        return 1;
    }
    if (runDashPrefixedTextValidationTest() != 0) {
        return 1;
    }
    if (runMissingSourceReferenceValidationTest() != 0) {
        return 1;
    }
    if (runUnsavedMissingSourceReferenceValidationTest() != 0) {
        return 1;
    }
    if (runProjectIndexDiagnosticProjectionTest() != 0) {
        return 1;
    }
    if (runDuplicateObjectIdDiagnosticIsNotDuplicatedTest() != 0) {
        return 1;
    }
    if (runRootConfigSourceGraphValidationTest() != 0) {
        return 1;
    }
    if (runProjectIndexUnavailableDiagnosticTest() != 0) {
        return 1;
    }
    if (runDocumentTypeContextProjectionTest() != 0) {
        return 1;
    }
    if (runSupersededScanStillDeliversLatestResultTest() != 0) {
        return 1;
    }
    if (runRepeatedRequestsDoNotStarveDebounceTest() != 0) {
        return 1;
    }
    if (runSupersededRunningScanStartsNextImmediatelyTest() != 0) {
        return 1;
    }
    if (runBabiceSampleValidationCompletesTest() != 0) {
        return 1;
    }
    if (runBabiceSampleValidationWithOpenDocumentsTest() != 0) {
        return 1;
    }
    return 0;
}
