#include "MapEditorTab.h"

#include "../TextEditorTab.h"

namespace TherionStudio
{
void MapEditorTab::initializeNewDocument(const QString &suggestedFileName, const QString &contents)
{
    clearDraftGeometryItems();
    clearBackgroundImageItems();
    textEditor_->initializeNewDocument(suggestedFileName, contents);
    refreshMapScene();
    fitMapToView();
    rebuildInspectorObjectsTree();
    refreshInspectorBackgroundPanel();
    refreshTitle();
    refreshStatus();
}

bool MapEditorTab::saveAs(const QString &filePath, QString *errorMessage)
{
    const bool saved = textEditor_->saveAs(filePath, errorMessage);
    if (saved) {
        refreshTitle();
        refreshStatus();
    }
    return saved;
}
}
