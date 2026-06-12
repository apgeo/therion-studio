#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
QString canonicalOrAbsoluteFilePath(const QString &path);
QStringList therionSourceReferencePathCandidates(const QString &currentFilePath, const QString &referencePath);
QString resolveTherionSourceReferencePath(const QString &currentFilePath, const QString &referencePath);
QString resolveTherionSourceReferencePath(const QString &currentFilePath,
                                          const QString &referencePath,
                                          const QSet<QString> &knownFilePaths);
}
