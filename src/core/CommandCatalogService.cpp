#include "CommandCatalogService.h"

#include <QFile>
#include <QJsonDocument>

namespace TherionStudio
{
const QJsonObject &CommandCatalogService::catalogObject()
{
    static const QJsonObject catalog = []() {
        QFile catalogFile(QStringLiteral(":/resources/therion_command_catalog.json"));
        if (!catalogFile.open(QIODevice::ReadOnly)) {
            return QJsonObject();
        }

        const QJsonDocument document = QJsonDocument::fromJson(catalogFile.readAll());
        if (!document.isObject()) {
            return QJsonObject();
        }

        return document.object();
    }();

    return catalog;
}

bool CommandCatalogService::isCatalogAvailable()
{
    return !catalogObject().isEmpty();
}
}
