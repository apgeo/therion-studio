#pragma once

#include <QSet>

namespace TherionStudio
{
class MapEditorTab;

class MapEditorSelectionController final
{
public:
    explicit MapEditorSelectionController(MapEditorTab *owner);

    void handleMapSceneSelectionChanged();
    void syncMapSelectionFromTextCursor(int lineNumber, int columnNumber);
    void updateGeometrySelectionPresentation();
    void selectMapLine(int lineNumber, bool centerOnSelection = true);
    void selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection = true);

private:
    MapEditorTab *owner_ = nullptr;
};
}
