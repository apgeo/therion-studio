#pragma once

#include <QJsonObject>

namespace TherionStudio
{
class CommandCatalogService final
{
public:
    static const QJsonObject &catalogObject();
    static bool isCatalogAvailable();
};
}
