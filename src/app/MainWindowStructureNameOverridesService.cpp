#include "MainWindowStructureNameOverridesService.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace TherionStudio
{
QHash<QString, QString> MainWindowStructureNameOverridesService::parse(const QString &json)
{
    QHash<QString, QString> overrides;
    if (json.trimmed().isEmpty()) {
        return overrides;
    }

    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) {
        return overrides;
    }

    const QJsonObject rootObject = document.object();
    for (auto iterator = rootObject.begin(); iterator != rootObject.end(); ++iterator) {
        const QString name = iterator.value().toString().trimmed();
        if (!name.isEmpty()) {
            overrides.insert(iterator.key(), name);
        }
    }

    return overrides;
}

QString MainWindowStructureNameOverridesService::serialize(const QHash<QString, QString> &overrides)
{
    QJsonObject rootObject;
    for (auto iterator = overrides.constBegin(); iterator != overrides.constEnd(); ++iterator) {
        rootObject.insert(iterator.key(), iterator.value());
    }

    return QString::fromUtf8(QJsonDocument(rootObject).toJson(QJsonDocument::Compact));
}
}
