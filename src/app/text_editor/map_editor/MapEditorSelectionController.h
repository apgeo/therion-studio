#pragma once

#include "MapEditorSelectionContext.h"

#include <QSet>

namespace TherionStudio
{
class MapEditorSelectionController final
{
public:
    explicit MapEditorSelectionController(MapEditorSelectionContext context);

    void handleMapSceneSelectionChanged();
    void syncMapSelectionFromTextCursor(int lineNumber, int columnNumber);
    void updateGeometrySelectionPresentation();
    void selectMapLine(int lineNumber, bool centerOnSelection = true);
    void selectMapLines(const QSet<int> &lineNumbers, bool centerOnSelection = true);

private:
    MapEditorSelectionContext context_;
};
}
