#include "../src/app/ProjectValidationController.h"
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
    };
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("scrap"), 1);
    catalog.commandRequiredPositionalCount.insert(QStringLiteral("line"), 1);
    catalog.commandOptionNames.insert(QStringLiteral("line"), {QStringLiteral("-clip")});
    catalog.commandOptionValueArityTokens.insert(commandOptionValueKey(QStringLiteral("line"), QStringLiteral("-clip")),
                                                 QStringLiteral("EXACTLY_ONE"));
    return catalog;
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

int runControllerForwardsTriggerAndSnapshotTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary project directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("live.th2"));
    if (!expect(writeTextFile(filePath,
                              QStringLiteral("scrap stale\n"
                                             "endscrap\n")),
                "Temporary project validation controller file could not be written.")) {
        return 1;
    }

    ProjectValidationController controller;
    controller.setDebounceIntervalMs(0);

    bool started = false;
    ProjectValidationController::Trigger startedTrigger = ProjectValidationController::Trigger::ManualRefresh;
    ProjectValidationScanner::Result finishedResult;
    bool finished = false;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(5000);

    QObject::connect(&controller,
                     &ProjectValidationController::validationStarted,
                     &loop,
                     [&](ProjectValidationController::Trigger trigger, quint64, const QString &) {
                         started = true;
                         startedTrigger = trigger;
                     });
    QObject::connect(&controller,
                     &ProjectValidationController::validationFinished,
                     &loop,
                     [&](const ProjectValidationScanner::Result &result) {
                         finished = true;
                         finishedResult = result;
                         loop.quit();
                     });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QHash<QString, QString> inMemoryContents;
    inMemoryContents.insert(canonicalOrAbsolutePath(filePath),
                            QStringLiteral("scrap live\n"
                                           "line wall -clip off \"-clip off\"\n"
                                           "endline\n"
                                           "endscrap\n"));

    ProjectValidationController::Request request;
    request.trigger = ProjectValidationController::Trigger::DocumentChanged;
    request.projectRootPath = tempDir.path();
    request.validationCatalog = testCatalog();
    request.inMemoryProjectContentsByPath = inMemoryContents;
    controller.requestValidation(request);

    timeout.start();
    loop.exec();

    if (!expect(started, "ProjectValidationController should emit validationStarted.")) {
        return 1;
    }
    if (!expect(startedTrigger == ProjectValidationController::Trigger::DocumentChanged,
                "ProjectValidationController should preserve the request trigger.")) {
        return 1;
    }
    if (!expect(finished, "ProjectValidationController should emit validationFinished.")) {
        return 1;
    }
    if (!expect(finishedResult.errorMessage.isEmpty(), "Project validation controller should not report an error.")) {
        return 1;
    }
    if (!expect(containsFinding(finishedResult, filePath, QStringLiteral("malformed-option-token")),
                "ProjectValidationController should pass unsaved in-memory document text to the scanner.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runControllerForwardsTriggerAndSnapshotTest() != 0) {
        return 1;
    }
    return 0;
}
