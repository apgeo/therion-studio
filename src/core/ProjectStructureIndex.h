#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace TherionStudio
{
enum class ProjectStructureEntryKind
{
    Unknown,
    Survey,
    Centreline,
    Map,
    Scrap,
    Station,
    Point,
    Line,
    Area,
};

enum class ProjectIndexDiagnosticKind
{
    UnknownMapScrapReference,
};

struct ProjectStructureEntry
{
    ProjectStructureEntryKind kind = ProjectStructureEntryKind::Unknown;
    QString objectId;
    QString parentObjectId;
    QString category;
    QString name;
    QString sourceFile;
    int lineNumber = 0;
    int depth = 0;
    bool createsNamespace = true;
};

struct ProjectIndexDiagnostic
{
    ProjectIndexDiagnosticKind kind = ProjectIndexDiagnosticKind::UnknownMapScrapReference;
    QString sourceObjectId;
    QString sourceFile;
    int lineNumber = 0;
    QString referencedName;
};

struct ProjectIndexSnapshot
{
    QVector<ProjectStructureEntry> entries;
    QHash<QString, QSet<QString>> mapScrapReferencesByMapKey;
    QVector<ProjectIndexDiagnostic> diagnostics;
};

class ProjectStructureIndex final
{
public:
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 QString *errorMessage = nullptr);
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                 QString *errorMessage = nullptr);
    static ProjectIndexSnapshot scanProjectIndex(const QString &projectRootPath,
                                                 const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                 const QString &preferredConfigPath,
                                                 QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath, QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanProject(const QString &projectRootPath,
                                                      const QHash<QString, QString> &inMemoryFileContentsByPath,
                                                      QString *errorMessage = nullptr);
    static QVector<ProjectStructureEntry> scanTh2Objects(const QString &sourceFile, const QString &text);
    static QString structureEntryNodeKey(const ProjectStructureEntry &entry);
};
}
