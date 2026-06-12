#include "../src/app/ProjectValidationScanner.h"
#include "../src/core/TherionCommandSyntax.h"

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
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
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
    catalog.commandContexts.insert(QStringLiteral("cs"), {QStringLiteral("centerline"), QStringLiteral("none")});
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
    catalog.commandDocumentTypes.insert(QStringLiteral("join"), {QStringLiteral("th")});
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
    if (runDocumentTypeContextProjectionTest() != 0) {
        return 1;
    }
    return 0;
}
