#include "CommandCatalogService.h"

#include <QFile>
#include <QJsonDocument>

#include <utility>

namespace TherionStudio
{
CommandCatalogStore::CommandCatalogStore()
    : CommandCatalogStore(fromFile(QStringLiteral(":/resources/therion_command_catalog.json")))
{
}

CommandCatalogStore::CommandCatalogStore(QJsonObject catalogObject)
    : catalogObject_(std::move(catalogObject))
{
}

CommandCatalogStore CommandCatalogStore::fromJsonBytes(const QByteArray &jsonBytes)
{
    const QJsonDocument document = QJsonDocument::fromJson(jsonBytes);
    if (!document.isObject()) {
        return CommandCatalogStore(QJsonObject());
    }

    return CommandCatalogStore(document.object());
}

CommandCatalogStore CommandCatalogStore::fromFile(const QString &filePath)
{
    QFile catalogFile(filePath);
    if (!catalogFile.open(QIODevice::ReadOnly)) {
        return CommandCatalogStore(QJsonObject());
    }

    return fromJsonBytes(catalogFile.readAll());
}

const QJsonObject &CommandCatalogStore::catalogObject() const
{
    return catalogObject_;
}

bool CommandCatalogStore::isCatalogAvailable() const
{
    return !catalogObject_.isEmpty();
}

const QJsonObject &CommandCatalogService::catalogObject()
{
    static const CommandCatalogStore catalogStore;
    return catalogStore.catalogObject();
}

bool CommandCatalogService::isCatalogAvailable()
{
    return !catalogObject().isEmpty();
}
}
