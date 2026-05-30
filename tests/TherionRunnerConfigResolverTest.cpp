#include "../src/app/TherionRunnerConfigResolver.h"

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

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

int runWorkingDirectoryResolutionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    const QDir root(tempDir.path());
    if (!expect(root.mkpath(QStringLiteral("project")) && root.mkpath(QStringLiteral("typed")),
                "Temporary working directories could not be created.")) {
        return 1;
    }

    const QString projectPath = root.filePath(QStringLiteral("project"));
    const QString typedPath = root.filePath(QStringLiteral("typed"));
    if (!expect(TherionRunnerConfigResolver::resolveWorkingDirectory(QString(), projectPath)
                    == QDir(projectPath).absolutePath(),
                "Project root should be used when no working directory is typed.")) {
        return 1;
    }

    if (!expect(TherionRunnerConfigResolver::resolveWorkingDirectory(typedPath, projectPath)
                    == QDir(typedPath).absolutePath(),
                "Typed working directory should take precedence over project root.")) {
        return 1;
    }

    return 0;
}

int runExplicitConfigArgumentDetectionTest()
{
    if (!expect(TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("-c"),
                                                                        QStringLiteral("custom.thconfig")}),
                "Separate -c argument should be treated as an explicit config argument.")) {
        return 1;
    }
    if (!expect(TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("--config=custom.thconfig")}),
                "--config= argument should be treated as an explicit config argument.")) {
        return 1;
    }
    if (!expect(TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("-c=custom.thconfig")}),
                "-c= argument should be treated as an explicit config argument.")) {
        return 1;
    }
    if (!expect(!TherionRunnerConfigResolver::hasExplicitConfigArgument({QStringLiteral("-q"),
                                                                         QStringLiteral("survey.th")}),
                "Non-config arguments should not be treated as explicit config arguments.")) {
        return 1;
    }

    return 0;
}

int runConfigPathResolutionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    QDir root(tempDir.path());
    if (!expect(root.mkpath(QStringLiteral("project")) && root.mkpath(QStringLiteral("work")),
                "Temporary config directories could not be created.")) {
        return 1;
    }

    const QString projectPath = root.filePath(QStringLiteral("project"));
    const QString workingDirectory = root.filePath(QStringLiteral("work"));
    const QString explicitConfigPath = QDir(workingDirectory).filePath(QStringLiteral("custom.thconfig"));
    const QString activeConfigPath = QDir(projectPath).filePath(QStringLiteral("active.thconfig"));
    const QString defaultConfigPath = QDir(workingDirectory).filePath(QStringLiteral("thconfig"));

    if (!expect(writeTextFile(explicitConfigPath, QStringLiteral("source custom.th\n")),
                "Explicit config fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(activeConfigPath, QStringLiteral("source active.th\n")),
                "Active config fixture could not be written.")) {
        return 1;
    }
    if (!expect(writeTextFile(defaultConfigPath, QStringLiteral("source default.th\n")),
                "Default config fixture could not be written.")) {
        return 1;
    }

    const QString resolvedExplicitConfig =
        TherionRunnerConfigResolver::resolveConfigPath({QStringLiteral("--config=custom.thconfig")},
                                                       workingDirectory,
                                                       projectPath,
                                                       activeConfigPath);
    if (!expect(canonicalOrAbsolutePath(resolvedExplicitConfig) == canonicalOrAbsolutePath(explicitConfigPath),
                "Explicit config argument should resolve relative to the working directory.")) {
        return 1;
    }

    const QString resolvedActiveConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       projectPath,
                                                       activeConfigPath);
    if (!expect(canonicalOrAbsolutePath(resolvedActiveConfig) == canonicalOrAbsolutePath(activeConfigPath),
                "Active .thconfig document should be used before default thconfig lookup.")) {
        return 1;
    }

    const QString resolvedDefaultConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       projectPath,
                                                       QDir(projectPath).filePath(QStringLiteral("survey.th")));
    if (!expect(canonicalOrAbsolutePath(resolvedDefaultConfig) == canonicalOrAbsolutePath(defaultConfigPath),
                "Default thconfig should resolve from the working directory.")) {
        return 1;
    }

    return 0;
}

int runThconfigWorkResolutionTest()
{
    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "Temporary directory creation failed.")) {
        return 1;
    }

    QDir root(tempDir.path());
    if (!expect(root.mkpath(QStringLiteral("work")),
                "Temporary working directory could not be created.")) {
        return 1;
    }

    const QString workingDirectory = root.filePath(QStringLiteral("work"));
    const QString namedConfigPath = QDir(workingDirectory).filePath(QStringLiteral("thconfig.work"));
    if (!expect(writeTextFile(namedConfigPath, QStringLiteral("source work.th\n")),
                "thconfig.work fixture could not be written.")) {
        return 1;
    }

    const QString resolvedNamedConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       QString(),
                                                       QString());
    if (!expect(canonicalOrAbsolutePath(resolvedNamedConfig) == canonicalOrAbsolutePath(namedConfigPath),
                "A single thconfig.* file should resolve as the default config when thconfig is absent.")) {
        return 1;
    }

    const QString secondConfigPath = QDir(workingDirectory).filePath(QStringLiteral("thconfig.debug"));
    if (!expect(writeTextFile(secondConfigPath, QStringLiteral("source debug.th\n")),
                "Second thconfig.* fixture could not be written.")) {
        return 1;
    }

    const QString ambiguousConfig =
        TherionRunnerConfigResolver::resolveConfigPath({},
                                                       workingDirectory,
                                                       QString(),
                                                       QString());
    if (!expect(ambiguousConfig.isEmpty(),
                "Multiple named thconfig.* files should not be auto-selected.")) {
        return 1;
    }

    return 0;
}
}

int main()
{
    if (runWorkingDirectoryResolutionTest() != 0) {
        return 1;
    }
    if (runExplicitConfigArgumentDetectionTest() != 0) {
        return 1;
    }
    if (runConfigPathResolutionTest() != 0) {
        return 1;
    }
    if (runThconfigWorkResolutionTest() != 0) {
        return 1;
    }

    return 0;
}
