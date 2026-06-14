#include "ApplicationIdentity.h"

namespace TherionStudio::Platform
{

const ApplicationIdentity &applicationIdentity()
{
    static const ApplicationIdentity identity{
        QStringLiteral("lblazek"),
        QStringLiteral("lblazek.com"),
        QStringLiteral("therionstudio"),
        QStringLiteral("Therion Studio"),
    };
    return identity;
}

} // namespace TherionStudio::Platform
