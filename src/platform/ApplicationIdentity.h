#pragma once

#include <QString>

namespace TherionStudio::Platform
{

struct ApplicationIdentity
{
    QString organizationName;
    QString organizationDomain;
    QString applicationName;
    QString applicationDisplayName;
};

const ApplicationIdentity &applicationIdentity();

} // namespace TherionStudio::Platform
