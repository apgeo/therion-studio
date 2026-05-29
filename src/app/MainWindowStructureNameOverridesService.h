#pragma once

#include <QHash>
#include <QString>

namespace TherionStudio
{
class MainWindowStructureNameOverridesService final
{
public:
    static QHash<QString, QString> parse(const QString &json);
    static QString serialize(const QHash<QString, QString> &overrides);
};
}
