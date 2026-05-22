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
}
