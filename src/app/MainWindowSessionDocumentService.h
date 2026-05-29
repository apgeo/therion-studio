#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace TherionStudio
{
class MainWindowSessionDocumentService final
{
public:
    enum class RestoreTarget
    {
        TextEditor,
        MapEditor
    };

    struct RestoreEntry
    {
        QString filePath;
        RestoreTarget target = RestoreTarget::TextEditor;
    };

    static std::vector<RestoreEntry> buildRestoreEntries(const QStringList &openDocumentPaths);
    static QStringList mergeOpenDocumentPaths(const QStringList &tabDocumentPaths,
                                              const QStringList &detachedMapDocumentPaths);
    static bool isMapDocumentPath(const QString &filePath);
};
}
