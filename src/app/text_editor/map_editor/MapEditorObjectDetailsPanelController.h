#pragma once

#include "MapEditorTab.h"

namespace TherionStudio
{
class MapEditorObjectDetailsPanelController final
{
public:
    explicit MapEditorObjectDetailsPanelController(MapEditorTab *owner);

    void refreshObjectDetailsPanel();

private:
    MapEditorTab *owner_ = nullptr;
};
}
