#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
struct TherionSourceValidationCatalog
{
    QSet<QString> commandNames;
    QHash<QString, QStringList> commandContexts;
    QHash<QString, QSet<QString>> commandOptionNames;
    QHash<QString, int> commandRequiredPositionalCount;
    QHash<QString, QStringList> commandArgumentAllowedValuesByKey;
    QHash<QString, QStringList> commandTypeValues;
    QHash<QString, QStringList> commandOptionAllowedValuesByKey;
    QHash<QString, QStringList> commandSubtypeValuesByTypeKey;
    QHash<QString, QString> commandOptionValueArityTokens;
    QHash<QString, int> commandOptionFixedArityByKey;
};
}
