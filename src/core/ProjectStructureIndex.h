#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace TherionStudio
{
struct ProjectStructureEntry
{
    QString objectId;
    QString parentObjectId;
    QString category;
    QString name;
    QString sourceFile;
    int lineNumber = 0;
    int depth = 0;
    bool createsNamespace = true;
};

struct ProjectIndexSnapshot
{
    QVector<ProjectStructureEntry> entries;
    QHash<QString, QSet<QString>> mapScrapReferencesByMapKey;
};

class ProjectStructureIndex final
{
public:
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 QString *errorMessage = nullptr);
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                 QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath, QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath,
                                                      const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                      QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanTh2Objects(const QString &sourceFile, const QString &text);
    static QString structureEntryNodeKey(const ProjectStructureEntry &entry);
};
}
