#include "../PlatformPathResolver.h"

#include <QDir>
#include <QtGlobal>

namespace TherionStudio::Platform
{
QStringList therionExecutableCandidates()
{
    QStringList candidates;

    const QString brewPrefix = qEnvironmentVariable("HOMEBREW_PREFIX").trimmed();
    if (!brewPrefix.isEmpty()) {
        candidates.append(QDir(brewPrefix).absoluteFilePath(QStringLiteral("bin/therion")));
    }

    candidates.append(QStringLiteral("/opt/homebrew/bin/therion"));
    candidates.append(QStringLiteral("/usr/local/bin/therion"));
    return candidates;
}

QStringList externalToolSearchPathCandidates()
{
    QStringList candidates;

    const QString brewPrefix = qEnvironmentVariable("HOMEBREW_PREFIX").trimmed();
    if (!brewPrefix.isEmpty()) {
        candidates.append(QDir(brewPrefix).absoluteFilePath(QStringLiteral("bin")));
        candidates.append(QDir(brewPrefix).absoluteFilePath(QStringLiteral("sbin")));
    }

    candidates.append(QStringLiteral("/opt/homebrew/bin"));
    candidates.append(QStringLiteral("/opt/homebrew/sbin"));
    candidates.append(QStringLiteral("/usr/local/bin"));
    candidates.append(QStringLiteral("/usr/local/sbin"));
    candidates.append(QStringLiteral("/Library/TeX/texbin"));
    candidates.append(QStringLiteral("/usr/texbin"));
    return candidates;
}
}
