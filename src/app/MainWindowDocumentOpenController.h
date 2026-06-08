#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace TherionStudio
{
class MainWindowDocumentOpenController final
{
public:
    struct IndexedDocumentPath
    {
        int index = -1;
        QString documentPath;
    };

    enum class OpenCurrentInMapAction
    {
        NoActiveDocument,
        UnsupportedDocument,
        OpenMapDocument
    };

    struct OpenCurrentInMapPlan
    {
        OpenCurrentInMapAction action = OpenCurrentInMapAction::NoActiveDocument;
        QString documentPath;
    };

    enum class OpenTextTabAction
    {
        UnsupportedDocument,
        ReuseAttachedTab,
        OpenNewTab
    };

    struct OpenTextTabPlan
    {
        OpenTextTabAction action = OpenTextTabAction::UnsupportedDocument;
        QString canonicalPath;
        int reuseTabIndex = -1;
    };

    enum class OpenMapTabAction
    {
        FocusDetachedTab,
        ReuseAttachedTab,
        OpenNewTab
    };

    struct OpenMapTabPlan
    {
        OpenMapTabAction action = OpenMapTabAction::OpenNewTab;
        QString canonicalPath;
        int reuseTabIndex = -1;
    };

    enum class OpenProjectSearchResultAction
    {
        OpenTextDocument,
        OpenMapDocument
    };

    struct OpenProjectSearchResultPlan
    {
        OpenProjectSearchResultAction action = OpenProjectSearchResultAction::OpenTextDocument;
        QString canonicalPath;
    };

    static QString canonicalDocumentPath(const QString &filePath);
    static bool isMapDocumentPath(const QString &filePath);

    static OpenCurrentInMapPlan planOpenCurrentDocumentInMap(bool hasCurrentDocument,
                                                             const QString &currentDocumentPath);

    static OpenTextTabPlan planOpenTextTab(const QString &filePath,
                                           bool supportedTextEditorPath,
                                           const QList<IndexedDocumentPath> &attachedTextTabs);

    static OpenMapTabPlan planOpenMapTab(const QString &filePath,
                                         const QStringList &detachedMapDocumentPaths,
                                         const QList<IndexedDocumentPath> &attachedMapTabs);

    static OpenProjectSearchResultPlan planOpenProjectSearchResult(const QString &filePath);
};
}
