#pragma once

#include <QString>
#include <QStringConverter>

namespace TherionStudio
{
class DocumentFile final
{
public:
    static bool readTextFile(const QString &filePath,
                             QString *contents,
                             QStringConverter::Encoding *encoding,
                             QString *encodingLabel,
                             QString *errorMessage);
    static bool writeTextFile(const QString &filePath,
                              const QString &contents,
                              QStringConverter::Encoding encoding,
                              QString *errorMessage);
    static bool readUtf8TextFile(const QString &filePath, QString *contents, QString *errorMessage);
    static bool writeUtf8TextFile(const QString &filePath, const QString &contents, QString *errorMessage);
};
}
