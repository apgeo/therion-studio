#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class MainWindowRecentProjectsService final
{
public:
    static constexpr int maxRecentProjectCount = 5;

    static QString normalizedProjectPath(const QString &projectPath);
    static QStringList normalizedRecentProjectPaths(const QStringList &projectPaths);
    static QStringList recordOpenedProject(const QStringList &currentProjectPaths,
                                           const QString &openedProjectPath);
    static QString projectDisplayName(const QString &projectPath);
};
}
