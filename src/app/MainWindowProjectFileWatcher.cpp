#include "MainWindow.h"

#include "../core/TherionFileTypes.h"

#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QFileSystemWatcher>

namespace
{
bool shouldSkipProjectWatchDirectory(const QFileInfo &info)
{
    const QString name = info.fileName();
    return name == QStringLiteral(".git")
        || name == QStringLiteral(".svn")
        || name == QStringLiteral(".hg")
        || name == QStringLiteral("CMakeFiles")
        || name == QStringLiteral("build")
        || name.startsWith(QStringLiteral("cmake-build"));
}

QString canonicalOrAbsolutePath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    return canonicalPath.isEmpty() ? info.absoluteFilePath() : canonicalPath;
}

bool isProjectValidationFile(const QFileInfo &info)
{
    if (!info.isFile()) {
        return false;
    }
    if (TherionStudio::isTherionConfigFileName(info.fileName())) {
        return true;
    }

    const QString suffix = info.suffix().toLower();
    return suffix == QStringLiteral("th")
        || suffix == QStringLiteral("th2");
}

void collectProjectWatchPaths(const QString &directoryPath, QStringList *directories, QStringList *files)
{
    if (directories == nullptr || files == nullptr) {
        return;
    }

    const QFileInfo directoryInfo(directoryPath);
    if (!directoryInfo.isDir()
        || directoryInfo.isSymLink()
        || shouldSkipProjectWatchDirectory(directoryInfo)) {
        return;
    }

    directories->append(canonicalOrAbsolutePath(directoryInfo.absoluteFilePath()));

    const QFileInfoList entries = QDir(directoryInfo.absoluteFilePath()).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectProjectWatchPaths(entry.absoluteFilePath(), directories, files);
        } else if (isProjectValidationFile(entry)) {
            files->append(canonicalOrAbsolutePath(entry.absoluteFilePath()));
        }
    }
}
}

void MainWindow::rebuildProjectFileWatcher()
{
    if (projectFileWatcher_ == nullptr) {
        return;
    }

    clearProjectFileWatcher();
    if (projectRootPath_.trimmed().isEmpty() || !QDir(projectRootPath_).exists()) {
        return;
    }

    QStringList directories;
    QStringList files;
    collectProjectWatchPaths(projectRootPath_, &directories, &files);
    directories.removeDuplicates();
    files.removeDuplicates();

    if (!directories.isEmpty()) {
        projectFileWatcher_->addPaths(directories);
    }
    if (!files.isEmpty()) {
        projectFileWatcher_->addPaths(files);
    }
}

void MainWindow::clearProjectFileWatcher()
{
    if (projectFileWatcher_ == nullptr) {
        return;
    }

    const QStringList directories = projectFileWatcher_->directories();
    if (!directories.isEmpty()) {
        projectFileWatcher_->removePaths(directories);
    }

    const QStringList files = projectFileWatcher_->files();
    if (!files.isEmpty()) {
        projectFileWatcher_->removePaths(files);
    }
}

void MainWindow::handleProjectDirectoryChanged(const QString &directoryPath)
{
    if (!isDocumentPathInsideOpenProject(directoryPath)) {
        return;
    }

    rebuildProjectFileWatcher();
    requestProjectValidationForFileSystemChange(directoryPath);
}

void MainWindow::handleProjectFileChanged(const QString &filePath)
{
    if (!isDocumentPathInsideOpenProject(filePath)) {
        return;
    }

    const QString normalizedPath = canonicalOrAbsolutePath(filePath);
    if (QFileInfo(normalizedPath).exists()
        && projectFileWatcher_ != nullptr
        && !projectFileWatcher_->files().contains(normalizedPath)) {
        projectFileWatcher_->addPath(normalizedPath);
    }

    requestProjectValidationForFileSystemChange(filePath);
}

void MainWindow::requestProjectValidationForFileSystemChange(const QString &changedPath)
{
    if (!isDocumentPathInsideOpenProject(changedPath)) {
        return;
    }

    requestProjectValidation(TherionStudio::ProjectValidationController::Trigger::ProjectFilesChanged,
                             false);
}
