#include "../src/app/TherionExecutableSelectionController.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

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

bool writeTextFile(const QString &filePath, const QString &contents = QString())
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    return file.write(contents.toUtf8()) == contents.toUtf8().size();
}

int runInitialBrowsePathTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("candidate.txt"));
    if (!expect(writeTextFile(filePath), "Temporary browse candidate could not be written.")) {
        return 1;
    }

    if (!expect(TherionExecutableSelectionController::initialBrowsePath(QString()) == QDir::homePath(),
                "Empty executable text should browse from the home directory.")) {
        return 1;
    }
    if (!expect(TherionExecutableSelectionController::initialBrowsePath(QDir(tempDir.path()).filePath(QStringLiteral("missing")))
                    == QDir::homePath(),
                "Missing executable text should browse from the home directory.")) {
        return 1;
    }
    if (!expect(TherionExecutableSelectionController::initialBrowsePath(filePath) == tempDir.path(),
                "Existing file executable text should browse from the file parent directory.")) {
        return 1;
    }
    if (!expect(TherionExecutableSelectionController::initialBrowsePath(tempDir.path()) == tempDir.path(),
                "Existing directory executable text should browse from that directory.")) {
        return 1;
    }

    return 0;
}

int runEmptySelectionTest()
{
    const TherionExecutableSelectionController::SelectionResult result =
        TherionExecutableSelectionController::evaluateSelection(QString());

    if (!expect(!result.isAccepted, "Empty selection should not be accepted.")) {
        return 1;
    }
    if (!expect(!result.showWarningDialog, "Empty selection should not show a warning.")) {
        return 1;
    }
    if (!expect(!result.shouldUpdateExecutableText, "Empty selection should not update executable text.")) {
        return 1;
    }

    return 0;
}

int runNonExecutableSelectionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QString filePath = QDir(tempDir.path()).filePath(QStringLiteral("not-executable.txt"));
    if (!expect(writeTextFile(filePath, QStringLiteral("not executable\n")),
                "Temporary non-executable candidate could not be written.")) {
        return 1;
    }
    if (!expect(!QFileInfo(filePath).isExecutable(),
                "Temporary text fixture unexpectedly appears executable on this platform.")) {
        return 1;
    }

    const TherionExecutableSelectionController::SelectionResult result =
        TherionExecutableSelectionController::evaluateSelection(filePath);

    if (!expect(!result.isAccepted, "Non-executable selection should not be accepted.")) {
        return 1;
    }
    if (!expect(result.showWarningDialog, "Non-executable selection should show a warning.")) {
        return 1;
    }
    if (!expect(result.warningDialogTitle == QStringLiteral("Select Therion Executable"),
                "Non-executable warning title is incorrect.")) {
        return 1;
    }
    if (!expect(result.warningDialogMessage == QStringLiteral("The selected file is not executable."),
                "Non-executable warning message is incorrect.")) {
        return 1;
    }

    return 0;
}

int runExecutableSelectionTest()
{
    const QString applicationPath = QCoreApplication::applicationFilePath();
    const QFileInfo applicationInfo(applicationPath);
    if (!expect(applicationInfo.isExecutable(), "The running test binary should be executable.")) {
        return 1;
    }

    const TherionExecutableSelectionController::SelectionResult result =
        TherionExecutableSelectionController::evaluateSelection(applicationPath);

    if (!expect(result.isAccepted, "Executable selection should be accepted.")) {
        return 1;
    }
    if (!expect(result.shouldUpdateExecutableText, "Executable selection should update executable text.")) {
        return 1;
    }
    if (!expect(result.updatedExecutableText == applicationInfo.absoluteFilePath(),
                "Executable selection should normalize to the absolute file path.")) {
        return 1;
    }
    if (!expect(result.shouldShowStatusBarMessage, "Executable selection should request a status-bar message.")) {
        return 1;
    }
    if (!expect(result.statusBarMessage == QStringLiteral("Therion executable updated"),
                "Executable selection status-bar message is incorrect.")) {
        return 1;
    }
    if (!expect(result.statusBarTimeoutMs == 2000, "Executable selection status timeout is incorrect.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runInitialBrowsePathTest() != 0) {
        return 1;
    }
    if (runEmptySelectionTest() != 0) {
        return 1;
    }
    if (runNonExecutableSelectionTest() != 0) {
        return 1;
    }
    if (runExecutableSelectionTest() != 0) {
        return 1;
    }

    return 0;
}
