#pragma once

#include "MapEditorObjectDetailsContext.h"

namespace TherionStudio
{
class MapEditorObjectDetailsPanelController final
{
public:
    explicit MapEditorObjectDetailsPanelController(MapEditorObjectDetailsContext context);

    void refreshObjectDetailsPanel();

private:
    QString translate(const char *text) const;

    MapEditorObjectDetailsContext context_;
};
}
