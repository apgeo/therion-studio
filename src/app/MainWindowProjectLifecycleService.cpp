#include "MainWindowProjectLifecycleService.h"

#include <QDir>

namespace TherionStudio
{
MainWindowProjectLifecycleService::OpenProjectDecision MainWindowProjectLifecycleService::decideOpenProject(const QString &selectedProjectPath)
{
    if (selectedProjectPath.isEmpty()) {
        return {};
    }

    if (!QDir(selectedProjectPath).exists()) {
        return {OpenProjectStatus::MissingDirectory, selectedProjectPath};
    }

    return {OpenProjectStatus::OpenProject, QDir(selectedProjectPath).absolutePath()};
}

MainWindowProjectLifecycleService::CloseProjectDecision MainWindowProjectLifecycleService::decideCloseProject(const QString &currentProjectPath,
                                                                                                               bool canCloseDirtyDocuments)
{
    if (currentProjectPath.isEmpty()) {
        return {};
    }

    if (!canCloseDirtyDocuments) {
        return {CloseProjectStatus::CancelledByDirtyDocuments, currentProjectPath};
    }

    return {CloseProjectStatus::CloseProject, currentProjectPath};
}

MainWindowProjectLifecycleService::CloseProjectDecision MainWindowProjectLifecycleService::decideCloseProject(const QString &currentProjectPath,
                                                                                                               const std::function<bool()> &confirmCloseDirtyDocuments)
{
    if (currentProjectPath.isEmpty()) {
        return {};
    }

    if (!confirmCloseDirtyDocuments || !confirmCloseDirtyDocuments()) {
        return {CloseProjectStatus::CancelledByDirtyDocuments, currentProjectPath};
    }

    return {CloseProjectStatus::CloseProject, currentProjectPath};
}
}
