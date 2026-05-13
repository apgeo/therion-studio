#pragma once

#include <QString>

namespace TherionStudio
{
class TherionDocumentEditor final
{
public:
    static bool rewriteStructureEntryName(QString *contents,
                                          int lineNumber,
                                          const QString &category,
                                          const QString &newName,
                                          QString *errorMessage = nullptr);
};
}