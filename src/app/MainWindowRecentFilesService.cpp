#include "MainWindowRecentFilesService.h"

#include <QDir>
#include <QFileInfo>

namespace TherionStudio
{
QString MainWindowRecentFilesService::normalizedProjectPath(const QString &projectPath)
{
    const QString trimmedPath = projectPath.trimmed();
    if (trimmedPath.isEmpty()) {
        return {};
    }

    return QDir::cleanPath(QFileInfo(trimmedPath).absoluteFilePath());
}

QString MainWindowRecentFilesService::normalizedFilePath(const QString &filePath)
{
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty()) {
        return {};
    }

    return QDir::cleanPath(QFileInfo(trimmedPath).absoluteFilePath());
}

bool MainWindowRecentFilesService::isFileInProject(const QString &projectPath, const QString &filePath)
{
    const QString normalizedProject = normalizedProjectPath(projectPath);
    const QString normalizedFile = normalizedFilePath(filePath);
    if (normalizedProject.isEmpty() || normalizedFile.isEmpty()) {
        return false;
    }

    const QString relativePath = QDir(normalizedProject).relativeFilePath(normalizedFile);
    return !relativePath.isEmpty()
        && relativePath != QStringLiteral(".")
        && !relativePath.startsWith(QStringLiteral("../"))
        && relativePath != QStringLiteral("..")
        && !QDir::isAbsolutePath(relativePath);
}

QStringList MainWindowRecentFilesService::normalizedRecentFilePaths(const QString &projectPath,
                                                                    const QStringList &filePaths)
{
    QStringList normalizedPaths;
    for (const QString &filePath : filePaths) {
        const QString normalizedPath = normalizedFilePath(filePath);
        if (normalizedPath.isEmpty()
            || !isFileInProject(projectPath, normalizedPath)
            || normalizedPaths.contains(normalizedPath)) {
            continue;
        }

        normalizedPaths.append(normalizedPath);
        if (normalizedPaths.size() == maxRecentFileCount) {
            break;
        }
    }

    return normalizedPaths;
}

QStringList MainWindowRecentFilesService::recordOpenedFile(const QString &projectPath,
                                                           const QStringList &currentFilePaths,
                                                           const QString &openedFilePath)
{
    const QString normalizedOpenedPath = normalizedFilePath(openedFilePath);
    if (!isFileInProject(projectPath, normalizedOpenedPath)) {
        return normalizedRecentFilePaths(projectPath, currentFilePaths);
    }

    QStringList updatedPaths;
    updatedPaths.append(normalizedOpenedPath);
    for (const QString &filePath : normalizedRecentFilePaths(projectPath, currentFilePaths)) {
        if (filePath == normalizedOpenedPath) {
            continue;
        }

        updatedPaths.append(filePath);
        if (updatedPaths.size() == maxRecentFileCount) {
            break;
        }
    }

    return updatedPaths;
}

QString MainWindowRecentFilesService::projectRelativeDisplayPath(const QString &projectPath,
                                                                 const QString &filePath)
{
    const QString normalizedProject = normalizedProjectPath(projectPath);
    const QString normalizedFile = normalizedFilePath(filePath);
    if (normalizedProject.isEmpty() || normalizedFile.isEmpty()) {
        return filePath;
    }

    return QDir::toNativeSeparators(QDir(normalizedProject).relativeFilePath(normalizedFile));
}
}
