#pragma once

#include <QString>

namespace TherionStudio
{
class DocumentFile final
{
public:
    static bool readUtf8TextFile(const QString &filePath, QString *contents, QString *errorMessage);
    static bool writeUtf8TextFile(const QString &filePath, const QString &contents, QString *errorMessage);
};
}
