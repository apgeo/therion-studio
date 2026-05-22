#include "TherionRunnerService.h"

#include "../platform/PlatformPathResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace
{
QString firstExecutableCandidatePath(const QStringList &candidatePaths)
{
    for (const QString &candidatePath : candidatePaths) {
        const QFileInfo candidateInfo(candidatePath);
        if (!candidateInfo.exists() || !candidateInfo.isFile() || !candidateInfo.isExecutable()) {
            continue;
        }

        const QString canonicalPath = candidateInfo.canonicalFilePath();
        return canonicalPath.isEmpty() ? candidateInfo.absoluteFilePath() : canonicalPath;
    }

    return QString();
}
}

namespace TherionStudio
{
TherionRunnerService::TherionRunnerService(QObject *parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &TherionRunnerService::handleReadyReadStandardOutput);
    connect(&process_, &QProcess::readyReadStandardError,
            this, &TherionRunnerService::handleReadyReadStandardError);
    connect(&process_, &QProcess::finished,
            this, &TherionRunnerService::handleFinished);
    connect(&process_, &QProcess::errorOccurred,
            this, &TherionRunnerService::handleError);
}

bool TherionRunnerService::isRunning() const
{
    return process_.state() != QProcess::NotRunning;
}

QString TherionRunnerService::suggestedDefaultExecutablePath()
{
    return firstExecutableCandidatePath(Platform::therionExecutableCandidates());
}

TherionRunnerService::StartResult TherionRunnerService::start(const QString &executableInput,
                                                              const QString &workingDirectory,
                                                              const QStringList &arguments)
{
    StartResult result;

    if (isRunning()) {
        result.code = StartCode::AlreadyRunning;
        return result;
    }

    if (workingDirectory.trimmed().isEmpty()) {
        result.code = StartCode::MissingWorkingDirectory;
        return result;
    }
    if (!QDir(workingDirectory).exists()) {
        result.code = StartCode::WorkingDirectoryNotFound;
        return result;
    }

    const ResolvedExecutablePath resolvedExecutable = resolveExecutablePath(executableInput, workingDirectory);
    const QFileInfo resolvedExecutableInfo(resolvedExecutable.path);
    if (resolvedExecutable.path.isEmpty()
        || !resolvedExecutableInfo.exists()
        || !resolvedExecutableInfo.isFile()
        || !resolvedExecutableInfo.isExecutable()) {
        result.code = StartCode::ExecutableNotFound;
        return result;
    }

    process_.setWorkingDirectory(workingDirectory);
    process_.start(resolvedExecutable.path, arguments);
    result.code = StartCode::Started;
    result.resolvedExecutablePath = resolvedExecutable.path;
    result.usedPlatformFallback = resolvedExecutable.usedPlatformFallback;
    emit runningStateChanged(isRunning());
    return result;
}

void TherionRunnerService::stop()
{
    if (!isRunning()) {
        return;
    }

    process_.kill();
}

QString TherionRunnerService::errorString() const
{
    return process_.errorString();
}

void TherionRunnerService::handleReadyReadStandardOutput()
{
    const QString output = QString::fromLocal8Bit(process_.readAllStandardOutput());
    if (!output.isEmpty()) {
        emit standardOutputReady(output);
    }
}

void TherionRunnerService::handleReadyReadStandardError()
{
    const QString output = QString::fromLocal8Bit(process_.readAllStandardError());
    if (!output.isEmpty()) {
        emit standardErrorReady(output);
    }
}

void TherionRunnerService::handleFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    emit runFinished(exitCode, exitStatus);
    emit runningStateChanged(isRunning());
}

void TherionRunnerService::handleError(QProcess::ProcessError)
{
    emit runError(process_.errorString());
    emit runningStateChanged(isRunning());
}

TherionRunnerService::ResolvedExecutablePath TherionRunnerService::resolveExecutablePath(const QString &executableInput,
                                                                                          const QString &workingDirectory) const
{
    ResolvedExecutablePath resolved;
    const QFileInfo executableInfo(executableInput);
    if (executableInfo.isAbsolute()) {
        resolved.path = executableInfo.absoluteFilePath();
        return resolved;
    }

    if (executableInput.contains(QLatin1Char('/')) || executableInput.contains(QLatin1Char('\\'))) {
        resolved.path = QDir(workingDirectory).absoluteFilePath(executableInput);
        return resolved;
    }

    resolved.path = QStandardPaths::findExecutable(executableInput);
    if (!resolved.path.isEmpty()) {
        return resolved;
    }

    resolved.path = firstExecutableCandidatePath(Platform::therionExecutableCandidates());
    resolved.usedPlatformFallback = !resolved.path.isEmpty();
    return resolved;
}
}
