#pragma once

#include <QHash>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct ProjectStructureEntry
{
    QString category;
    QString name;
    QString sourceFile;
    int lineNumber = 0;
    int depth = 0;
    bool createsNamespace = true;
};

class ProjectStructureIndex final
{
public:
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath, QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath,
                                                      const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                      QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanTh2Objects(const QString &sourceFile, const QString &text);
};
}
