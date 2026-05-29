#pragma once

#include <QString>

namespace TherionStudio
{
class MainWindowProjectLifecycleService final
{
public:
    enum class OpenProjectStatus
    {
        Cancelled,
        MissingDirectory,
        OpenProject
    };

    struct OpenProjectDecision
    {
        OpenProjectStatus status = OpenProjectStatus::Cancelled;
        QString projectPath;
    };

    enum class CloseProjectStatus
    {
        NoProjectOpen,
        CancelledByDirtyDocuments,
        CloseProject
    };

    struct CloseProjectDecision
    {
        CloseProjectStatus status = CloseProjectStatus::NoProjectOpen;
        QString closedProjectPath;
    };

    static OpenProjectDecision decideOpenProject(const QString &selectedProjectPath);
    static CloseProjectDecision decideCloseProject(const QString &currentProjectPath,
                                                   bool canCloseDirtyDocuments);
};
}
