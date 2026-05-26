#pragma once

#include "IFileSystem.h"

namespace TherionStudio
{
class QtFileSystem final : public IFileSystem
{
public:
    bool readTextFile(const QString &filePath,
                      QString *contents,
                      QString *encodingName,
                      QString *encodingLabel,
                      QString *errorMessage) override;

    bool writeTextFile(const QString &filePath,
                       const QString &contents,
                       const QString &encodingName,
                       QString *errorMessage) override;

    bool readUtf8TextFile(const QString &filePath,
                          QString *contents,
                          QString *errorMessage) override;

    bool writeUtf8TextFile(const QString &filePath,
                           const QString &contents,
                           QString *errorMessage) override;
};
}
