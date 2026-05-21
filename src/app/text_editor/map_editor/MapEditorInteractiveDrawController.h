#pragma once

#include "MapEditorTab.h"

#include <QPointF>

namespace TherionStudio
{
class MapEditorInteractiveDrawController final
{
public:
    explicit MapEditorInteractiveDrawController(MapEditorTab *owner);

    void setInteractiveDrawMode(MapEditorTab::InteractiveDrawMode mode);
    bool handleInteractiveDrawClick(const QPointF &scenePosition);
    bool commitInteractiveDrawSession();
    void clearInteractiveDrawSession(bool clearMode);
    void updateInteractiveDrawPreview();
    bool cancelInteractiveDrawingToSelectMode();

private:
    MapEditorTab *owner_ = nullptr;
};
}
