#pragma once

#include "MapEditorInspectorObjectContext.h"

#include <QModelIndex>
#include <QSet>

namespace TherionStudio
{
class MapEditorInspectorObjectController final
{
public:
    explicit MapEditorInspectorObjectController(MapEditorInspectorObjectContext context);

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
    QString translate(const char *text) const;

    MapEditorInspectorObjectContext context_;
};
}
