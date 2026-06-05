#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class MainWindowRecentFilesService final
{
public:
    static constexpr int maxRecentFileCount = 10;

    static QString normalizedProjectPath(const QString &projectPath);
    static QString normalizedFilePath(const QString &filePath);
    static bool isFileInProject(const QString &projectPath, const QString &filePath);
    static QStringList normalizedRecentFilePaths(const QString &projectPath,
                                                 const QStringList &filePaths);
    static QStringList recordOpenedFile(const QString &projectPath,
                                        const QStringList &currentFilePaths,
                                        const QString &openedFilePath);
    static QString projectRelativeDisplayPath(const QString &projectPath,
                                              const QString &filePath);
};
}
