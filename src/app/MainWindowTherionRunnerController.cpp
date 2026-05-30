#include "MainWindowTherionRunnerController.h"

#include "TherionRunnerConfigResolver.h"
#include "../core/TherionFileTypes.h"

#include <QFileInfo>
#include <QProcess>

namespace
{
constexpr auto kTherionRunTargetCurrent = "current";
constexpr auto kTherionRunTargetProject = "project";

QString canonicalOrAbsolutePath(const QString &path)
{
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        return QString();
    }

    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}

QString normalizeRunTargetMode(const QString &mode, bool currentConfigAvailable)
{
    if (currentConfigAvailable) {
        return QString::fromLatin1(kTherionRunTargetCurrent);
    }

    if (mode == QString::fromLatin1(kTherionRunTargetCurrent)
        || mode == QString::fromLatin1(kTherionRunTargetProject)) {
        return mode;
    }

    return QString::fromLatin1(kTherionRunTargetProject);
}
}

namespace TherionStudio
{
QString MainWindowTherionRunnerController::currentDocumentConfigPath(const QString &currentDocumentPath)
{
    if (currentDocumentPath.isEmpty() || !isTherionConfigFilePath(currentDocumentPath)) {
        return QString();
    }

    return canonicalOrAbsolutePath(currentDocumentPath);
}

MainWindowTherionRunnerController::RuntimeState
MainWindowTherionRunnerController::computeRuntimeState(const RuntimeInput &input)
{
    RuntimeState state;
    state.currentDocumentConfigPath = currentDocumentConfigPath(input.currentDocumentPath);
    state.currentConfigAvailable = !state.currentDocumentConfigPath.isEmpty();
    state.effectiveRunTargetMode = normalizeRunTargetMode(input.runTargetMode.trimmed(),
                                                          state.currentConfigAvailable);
    state.projectConfigActive = state.effectiveRunTargetMode != QString::fromLatin1(kTherionRunTargetCurrent);

    const QString workingDirectoryOverride = state.projectConfigActive
        ? input.workingDirectoryOverrideText.trimmed()
        : QString();
    state.hasWorkingDirectoryOverride = !workingDirectoryOverride.isEmpty();
    state.configResolutionDirectory = TherionRunnerConfigResolver::resolveWorkingDirectory(
        workingDirectoryOverride, input.projectRootPath);

    state.baseArguments = input.argumentsText.trimmed().isEmpty()
        ? QStringList()
        : QProcess::splitCommand(input.argumentsText.trimmed());
    state.hasExplicitConfigArgument = TherionRunnerConfigResolver::hasExplicitConfigArgument(state.baseArguments);

    if (!input.targetConfigPathText.trimmed().isEmpty()) {
        state.resolvedTargetConfigPath = TherionRunnerConfigResolver::resolveCandidatePath(
            input.targetConfigPathText.trimmed(),
            state.configResolutionDirectory,
            input.projectRootPath);
    }

    if (state.hasExplicitConfigArgument) {
        state.resolvedConfigPath = TherionRunnerConfigResolver::resolveConfigPath(
            state.baseArguments,
            state.configResolutionDirectory,
            input.projectRootPath,
            state.currentDocumentConfigPath);
    } else if (state.effectiveRunTargetMode == QString::fromLatin1(kTherionRunTargetCurrent)) {
        state.resolvedConfigPath = state.currentDocumentConfigPath;
    } else if (!state.resolvedTargetConfigPath.isEmpty()) {
        state.resolvedConfigPath = state.resolvedTargetConfigPath;
    } else {
        state.resolvedConfigPath = TherionRunnerConfigResolver::resolveConfigPath(
            QStringList(),
            state.configResolutionDirectory,
            input.projectRootPath,
            QString());
    }

    if (!workingDirectoryOverride.isEmpty()) {
        state.resolvedWorkingDirectory = TherionRunnerConfigResolver::resolveWorkingDirectory(
            workingDirectoryOverride, input.projectRootPath);
    } else if (state.hasExplicitConfigArgument) {
        state.resolvedWorkingDirectory = state.configResolutionDirectory;
    } else if (!state.resolvedConfigPath.isEmpty()) {
        state.resolvedWorkingDirectory = QFileInfo(state.resolvedConfigPath).absolutePath();
    } else {
        state.resolvedWorkingDirectory = TherionRunnerConfigResolver::resolveWorkingDirectory(
            QString(), input.projectRootPath);
    }

    state.executableInput = input.executableText.trimmed().isEmpty()
        ? QStringLiteral("therion")
        : input.executableText.trimmed();

    state.runArguments = state.baseArguments;
    if (!state.hasExplicitConfigArgument && !state.resolvedConfigPath.isEmpty()) {
        state.runArguments.append(state.resolvedConfigPath);
    }

    state.canRun = state.hasExplicitConfigArgument || !state.resolvedConfigPath.isEmpty();
    return state;
}
}
