#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class ProjectTemplateService
{
public:
    struct CreateProjectResult
    {
        bool success = false;
        QString projectRootPath;
        QString targetConfigPath;
        QStringList openFilePaths;
        QString errorMessage;
    };

    static QString defaultTemplateResourcePath();
    static CreateProjectResult createProjectFromTemplate(const QString &templateRootPath,
                                                         const QString &targetProjectPath);
};
}