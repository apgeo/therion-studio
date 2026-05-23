#include "../PlatformPathResolver.h"

namespace TherionStudio::Platform
{
QStringList therionExecutableCandidates()
{
    return {};
}

QStringList externalToolSearchPathCandidates()
{
    return {
        QStringLiteral("/usr/local/bin"),
        QStringLiteral("/usr/local/sbin"),
        QStringLiteral("/usr/bin"),
        QStringLiteral("/usr/sbin"),
        QStringLiteral("/bin"),
        QStringLiteral("/sbin"),
    };
}
}
