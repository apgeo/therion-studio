#pragma once

#include <QString>

namespace TherionStudio
{
class IFileSystem
{
public:
    virtual ~IFileSystem() = default;

    virtual bool readTextFile(const QString &filePath,
                              QString *contents,
                              QString *encodingName,
                              QString *encodingLabel,
                              QString *errorMessage) = 0;

    virtual bool writeTextFile(const QString &filePath,
                               const QString &contents,
                               const QString &encodingName,
                               QString *errorMessage) = 0;

    virtual bool readUtf8TextFile(const QString &filePath,
                                  QString *contents,
                                  QString *errorMessage) = 0;

    virtual bool writeUtf8TextFile(const QString &filePath,
                                   const QString &contents,
                                   QString *errorMessage) = 0;
};
}
