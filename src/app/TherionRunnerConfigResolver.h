#pragma once

#include <QString>
#include <QStringList>

namespace TherionStudio
{
class TherionRunnerConfigResolver final
{
public:
    static QString resolveWorkingDirectory(const QString &typedWorkingDirectory,
                                           const QString &projectRootPath);
    static bool hasExplicitConfigArgument(const QStringList &arguments);
    static QString resolveConfigPath(const QStringList &arguments,
                                     const QString &workingDirectory,
                                     const QString &projectRootPath,
                                     const QString &activeDocumentPath);

private:
    static QString resolveCandidatePath(const QString &candidate,
                                        const QString &workingDirectory,
                                        const QString &projectRootPath);
};
}
