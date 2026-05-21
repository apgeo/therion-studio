#pragma once

#include "MapEditorTab.h"

#include <QModelIndex>
#include <QSet>

namespace TherionStudio
{
class MapEditorInspectorObjectController final
{
public:
    explicit MapEditorInspectorObjectController(MapEditorTab *owner);
    explicit MapEditorInspectorObjectController(const MapEditorTab *owner);

    void rebuildInspectorObjectsTree();
    void configureInspectorObjectTreeColumns();
    QModelIndex findInspectorObjectIndexForLine(int lineNumber) const;
    void syncInspectorObjectSelectionToLine(int lineNumber);
    void syncInspectorObjectSelectionToLine(int lineNumber, bool scrollToSelection);
    void setInspectorObjectCurrentIndex(const QModelIndex &index);
    void clearInspectorObjectSelection(const QSet<int> &suppressAutoReselectLineNumbers = {});
    void handleInspectorObjectSelectionChanged(const QModelIndex &current);
    void handleInspectorObjectClicked(const QModelIndex &index);
    void applyInspectorObjectVisibility();

private:
    MapEditorTab *owner_ = nullptr;
};
}
