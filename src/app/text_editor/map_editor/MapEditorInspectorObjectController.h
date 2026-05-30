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
    bool moveInspectorObject(const QModelIndex &sourceIndex, const QModelIndex &targetIndex, bool afterTarget);
    void applyInspectorObjectVisibility();

private:
    QString tr(const char *text) const;

    MapEditorInspectorObjectContext context_;
};
}
