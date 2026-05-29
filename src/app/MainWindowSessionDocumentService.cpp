#include "MainWindowSessionDocumentService.h"

#include <QFileInfo>

#include <utility>

namespace TherionStudio
{
std::vector<MainWindowSessionDocumentService::RestoreEntry> MainWindowSessionDocumentService::buildRestoreEntries(const QStringList &openDocumentPaths)
{
    std::vector<RestoreEntry> entries;
    entries.reserve(static_cast<size_t>(openDocumentPaths.size()));

    for (const QString &documentPath : openDocumentPaths) {
        if (documentPath.isEmpty()) {
            continue;
        }

        RestoreEntry entry;
        entry.filePath = documentPath;
        entry.target = isMapDocumentPath(documentPath) ? RestoreTarget::MapEditor : RestoreTarget::TextEditor;
        entries.push_back(std::move(entry));
    }

    return entries;
}

QStringList MainWindowSessionDocumentService::mergeOpenDocumentPaths(const QStringList &tabDocumentPaths,
                                                                     const QStringList &detachedMapDocumentPaths)
{
    QStringList mergedPaths;
    mergedPaths.reserve(tabDocumentPaths.size() + detachedMapDocumentPaths.size());

    for (const QString &documentPath : tabDocumentPaths) {
        if (!documentPath.isEmpty()) {
            mergedPaths.append(documentPath);
        }
    }

    for (const QString &documentPath : detachedMapDocumentPaths) {
        if (documentPath.isEmpty() || mergedPaths.contains(documentPath)) {
            continue;
        }
        mergedPaths.append(documentPath);
    }

    return mergedPaths;
}

MainWindowSessionDocumentService::OpenDocumentsState MainWindowSessionDocumentService::buildOpenDocumentsState(const QStringList &tabDocumentPaths,
                                                                                                                const QStringList &detachedMapDocumentPaths,
                                                                                                                const QStringList &activeDetachedDocumentPaths,
                                                                                                                const QString &currentDocumentPath)
{
    OpenDocumentsState state;
    state.openDocumentPaths = mergeOpenDocumentPaths(tabDocumentPaths, detachedMapDocumentPaths);

    for (const QString &detachedPath : activeDetachedDocumentPaths) {
        if (!detachedPath.isEmpty()) {
            state.activeDocumentPath = detachedPath;
            return state;
        }
    }

    state.activeDocumentPath = currentDocumentPath;
    return state;
}

bool MainWindowSessionDocumentService::isMapDocumentPath(const QString &filePath)
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("th2"), Qt::CaseInsensitive) == 0;
}
}
