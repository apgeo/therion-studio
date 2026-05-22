#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace TherionStudio
{
class CommandCatalogStore final
{
public:
    CommandCatalogStore();
    explicit CommandCatalogStore(QJsonObject catalogObject);

    static CommandCatalogStore fromJsonBytes(const QByteArray &jsonBytes);
    static CommandCatalogStore fromFile(const QString &filePath);

    const QJsonObject &catalogObject() const;
    bool isCatalogAvailable() const;

private:
    QJsonObject catalogObject_;
};

class CommandCatalogService final
{
public:
    static const QJsonObject &catalogObject();
    static bool isCatalogAvailable();
};
}
