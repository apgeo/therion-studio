#pragma once

#include <QByteArray>
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

inline bool isTherionProjectFileName(const QString &fileName)
{
    if (isTherionConfigFileName(fileName)) {
        return true;
    }

    const QString suffix = QFileInfo(fileName).suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

inline bool isTherionProjectFilePath(const QString &filePath)
{
    return isTherionProjectFileName(QFileInfo(filePath).fileName());
}

inline QByteArray initialTherionProjectFileContents(const QString &fileName)
{
    return isTherionProjectFileName(fileName)
        ? QByteArray("encoding utf-8\n")
        : QByteArray();
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
