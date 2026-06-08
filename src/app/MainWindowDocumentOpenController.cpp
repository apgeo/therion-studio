#include "MainWindowDocumentOpenController.h"

#include <QFileInfo>

namespace TherionStudio
{
QString MainWindowDocumentOpenController::canonicalDocumentPath(const QString &filePath)
{
    const QFileInfo fileInfo(filePath);
    const QString canonicalPath = fileInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? fileInfo.absoluteFilePath() : canonicalPath;
}

bool MainWindowDocumentOpenController::isMapDocumentPath(const QString &filePath)
{
    return filePath.endsWith(QStringLiteral(".th2"), Qt::CaseInsensitive);
}

MainWindowDocumentOpenController::OpenCurrentInMapPlan
MainWindowDocumentOpenController::planOpenCurrentDocumentInMap(bool hasCurrentDocument,
                                                               const QString &currentDocumentPath)
{
    OpenCurrentInMapPlan plan;
    if (!hasCurrentDocument) {
        return plan;
    }

    if (!isMapDocumentPath(currentDocumentPath)) {
        plan.action = OpenCurrentInMapAction::UnsupportedDocument;
        return plan;
    }

    plan.action = OpenCurrentInMapAction::OpenMapDocument;
    plan.documentPath = currentDocumentPath;
    return plan;
}

MainWindowDocumentOpenController::OpenTextTabPlan
MainWindowDocumentOpenController::planOpenTextTab(const QString &filePath,
                                                  bool supportedTextEditorPath,
                                                  const QList<IndexedDocumentPath> &attachedTextTabs)
{
    OpenTextTabPlan plan;
    plan.canonicalPath = canonicalDocumentPath(filePath);
    if (!supportedTextEditorPath) {
        return plan;
    }

    for (const IndexedDocumentPath &entry : attachedTextTabs) {
        if (entry.index >= 0
            && canonicalDocumentPath(entry.documentPath) == plan.canonicalPath) {
            plan.action = OpenTextTabAction::ReuseAttachedTab;
            plan.reuseTabIndex = entry.index;
            return plan;
        }
    }

    plan.action = OpenTextTabAction::OpenNewTab;
    return plan;
}

MainWindowDocumentOpenController::OpenMapTabPlan
MainWindowDocumentOpenController::planOpenMapTab(const QString &filePath,
                                                 const QStringList &detachedMapDocumentPaths,
                                                 const QList<IndexedDocumentPath> &attachedMapTabs)
{
    OpenMapTabPlan plan;
    plan.canonicalPath = canonicalDocumentPath(filePath);

    for (const QString &detachedPath : detachedMapDocumentPaths) {
        if (canonicalDocumentPath(detachedPath) == plan.canonicalPath) {
            plan.action = OpenMapTabAction::FocusDetachedTab;
            return plan;
        }
    }

    for (const IndexedDocumentPath &entry : attachedMapTabs) {
        if (entry.index >= 0
            && canonicalDocumentPath(entry.documentPath) == plan.canonicalPath) {
            plan.action = OpenMapTabAction::ReuseAttachedTab;
            plan.reuseTabIndex = entry.index;
            return plan;
        }
    }

    plan.action = OpenMapTabAction::OpenNewTab;
    return plan;
}

MainWindowDocumentOpenController::OpenProjectSearchResultPlan
MainWindowDocumentOpenController::planOpenProjectSearchResult(const QString &filePath)
{
    OpenProjectSearchResultPlan plan;
    plan.canonicalPath = canonicalDocumentPath(filePath);
    plan.action = isMapDocumentPath(filePath)
        ? OpenProjectSearchResultAction::OpenMapDocument
        : OpenProjectSearchResultAction::OpenTextDocument;
    return plan;
}
}
