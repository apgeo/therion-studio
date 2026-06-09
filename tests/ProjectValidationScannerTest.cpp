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
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-close")),
                                                 QStringLiteral("EXACTLY_ONE"));
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-clip")),
                                                 QStringLiteral("EXACTLY_ONE"));
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
    return 0;
}
