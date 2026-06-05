#include "MainWindowRecentProjectsService.h"

#include <QDir>
#include <QFileInfo>

namespace TherionStudio
{
QString MainWindowRecentProjectsService::normalizedProjectPath(const QString &projectPath)
{
    const QString trimmedPath = projectPath.trimmed();
    if (trimmedPath.isEmpty()) {
        return QString();
    }

    return QDir::cleanPath(QFileInfo(trimmedPath).absoluteFilePath());
}

QStringList MainWindowRecentProjectsService::normalizedRecentProjectPaths(const QStringList &projectPaths)
{
    QStringList normalizedPaths;
    for (const QString &projectPath : projectPaths) {
        const QString normalizedPath = normalizedProjectPath(projectPath);
        if (normalizedPath.isEmpty() || normalizedPaths.contains(normalizedPath)) {
            continue;
        }

        normalizedPaths.append(normalizedPath);
        if (normalizedPaths.size() == maxRecentProjectCount) {
            break;
        }
    }

    return normalizedPaths;
}

QStringList MainWindowRecentProjectsService::recordOpenedProject(const QStringList &currentProjectPaths,
                                                                 const QString &openedProjectPath)
{
    const QString normalizedOpenedPath = normalizedProjectPath(openedProjectPath);
    if (normalizedOpenedPath.isEmpty()) {
        return normalizedRecentProjectPaths(currentProjectPaths);
    }

    QStringList updatedPaths;
    updatedPaths.append(normalizedOpenedPath);
    for (const QString &projectPath : normalizedRecentProjectPaths(currentProjectPaths)) {
        if (projectPath == normalizedOpenedPath) {
            continue;
        }

        updatedPaths.append(projectPath);
        if (updatedPaths.size() == maxRecentProjectCount) {
            break;
        }
    }

    return updatedPaths;
}

QString MainWindowRecentProjectsService::projectDisplayName(const QString &projectPath)
{
    const QFileInfo projectInfo(normalizedProjectPath(projectPath));
    const QString fileName = projectInfo.fileName();
    return fileName.isEmpty() ? projectPath : fileName;
}
}
