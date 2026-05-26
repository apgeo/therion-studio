#include "QtFileSystem.h"

#include "DocumentFile.h"

namespace TherionStudio
{
bool QtFileSystem::readTextFile(const QString &filePath,
                                QString *contents,
                                QString *encodingName,
                                QString *encodingLabel,
                                QString *errorMessage)
{
    return DocumentFile::readTextFile(filePath, contents, encodingName, encodingLabel, errorMessage);
}

bool QtFileSystem::writeTextFile(const QString &filePath,
                                 const QString &contents,
                                 const QString &encodingName,
                                 QString *errorMessage)
{
    return DocumentFile::writeTextFile(filePath, contents, encodingName, errorMessage);
}

bool QtFileSystem::readUtf8TextFile(const QString &filePath,
                                    QString *contents,
                                    QString *errorMessage)
{
    return DocumentFile::readUtf8TextFile(filePath, contents, errorMessage);
}

bool QtFileSystem::writeUtf8TextFile(const QString &filePath,
                                     const QString &contents,
                                     QString *errorMessage)
{
    return DocumentFile::writeUtf8TextFile(filePath, contents, errorMessage);
}
}
