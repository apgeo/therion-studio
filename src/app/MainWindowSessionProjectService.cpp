#include "MainWindowSessionProjectService.h"

#include <QDir>
#include <QStandardPaths>

namespace TherionStudio
{
MainWindowSessionProjectService::ProjectRestoreDecision MainWindowSessionProjectService::decideProjectRestore(const QString &lastProjectPath)
{
    if (lastProjectPath.isEmpty() || !QDir(lastProjectPath).exists()) {
        return {};
    }

    if (isProtectedMacUserFolder(lastProjectPath)) {
        return {ProjectRestoreStatus::SkippedProtectedFolder, lastProjectPath};
    }

    return {ProjectRestoreStatus::Restored, lastProjectPath};
}

bool MainWindowSessionProjectService::isProtectedMacUserFolder(const QString &path)
{
    const QString canonicalPath = QDir(path).absolutePath();
    const QStringList protectedRoots = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)};

    for (const QString &protectedRoot : protectedRoots) {
        if (protectedRoot.isEmpty()) {
            continue;
        }

        const QString normalizedProtectedRoot = QDir(protectedRoot).absolutePath();
        if (canonicalPath == normalizedProtectedRoot || canonicalPath.startsWith(normalizedProtectedRoot + QLatin1Char('/'))) {
            return true;
        }
    }

    return false;
}
}
