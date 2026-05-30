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
    QString tr(const char *text) const;
    const InspectorSymbolCatalog &inspectorSymbolCatalog() const;
    const MapEditorOrientationApplicabilityByCommand &orientationApplicabilityByCommand() const;

    MapEditorObjectDetailsContext context_;
};
}
