#pragma once

#include <QFileInfo>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
inline bool isTherionConfigFileName(const QString &fileName)
{
    const QString normalized = fileName.trimmed().toLower();
    return normalized == QStringLiteral("thconfig")
        || normalized.startsWith(QStringLiteral("thconfig."))
        || normalized.endsWith(QStringLiteral(".thconfig"));
}

inline bool isTherionConfigFilePath(const QString &filePath)
{
    return isTherionConfigFileName(QFileInfo(filePath).fileName());
}

inline QStringList therionConfigNameFilters()
{
    return {
        QStringLiteral("thconfig"),
        QStringLiteral("thconfig.*"),
        QStringLiteral("*.thconfig"),
    };
}
}
