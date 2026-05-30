#include "ApplicationLanguageOverride.h"

namespace TherionStudio::Platform
{

QString applicationLanguageOverride()
{
    return QStringLiteral("system");
}

void setApplicationLanguageOverride(const QString &language)
{
    Q_UNUSED(language);
}

} // namespace TherionStudio::Platform
