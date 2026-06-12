#include "TherionSourceReferenceResolver.h"

#include <QDir>
#include <QFileInfo>

namespace TherionStudio
{
QString canonicalOrAbsoluteFilePath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return QString();
    }

    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    if (!canonicalPath.isEmpty()) {
        return canonicalPath;
    }

    const QFileInfo parentInfo(info.absolutePath());
    const QString canonicalParentPath = parentInfo.canonicalFilePath();
    if (!canonicalParentPath.isEmpty()) {
        return QDir(canonicalParentPath).filePath(info.fileName());
    }

    return info.absoluteFilePath();
}

QStringList therionSourceReferencePathCandidates(const QString &currentFilePath, const QString &referencePath)
{
    const QString trimmedReference = referencePath.trimmed();
    if (trimmedReference.isEmpty()) {
        return {};
    }

    QStringList candidates;
    const QDir currentDirectory = QFileInfo(currentFilePath).dir();
    auto appendCandidate = [&candidates](const QString &path) {
        const QString normalizedPath = canonicalOrAbsoluteFilePath(path);
        if (!normalizedPath.isEmpty() && !candidates.contains(normalizedPath)) {
            candidates.append(normalizedPath);
        }
    };

    appendCandidate(currentDirectory.filePath(trimmedReference));

    const QFileInfo absoluteCandidate(trimmedReference);
    if (absoluteCandidate.isAbsolute()) {
        appendCandidate(trimmedReference);
    }

    if (QFileInfo(trimmedReference).suffix().isEmpty()) {
        appendCandidate(currentDirectory.filePath(trimmedReference + QStringLiteral(".th")));
    }

    return candidates;
}

QString resolveTherionSourceReferencePath(const QString &currentFilePath, const QString &referencePath)
{
    for (const QString &candidate : therionSourceReferencePathCandidates(currentFilePath, referencePath)) {
        if (QFileInfo(candidate).exists()) {
            return candidate;
        }
    }

    return QString();
}

QString resolveTherionSourceReferencePath(const QString &currentFilePath,
                                          const QString &referencePath,
                                          const QSet<QString> &knownFilePaths)
{
    for (const QString &candidate : therionSourceReferencePathCandidates(currentFilePath, referencePath)) {
        if (knownFilePaths.contains(candidate) || QFileInfo(candidate).exists()) {
            return candidate;
        }
    }

    return QString();
}
}
