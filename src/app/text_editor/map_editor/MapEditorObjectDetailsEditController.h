#pragma once

#include "MapEditorTab.h"

namespace TherionStudio
{
class MapEditorObjectDetailsEditController final
{
public:
    explicit MapEditorObjectDetailsEditController(MapEditorTab *owner);

    void applyObjectOrientationEdits();
    void handleObjectOrientationEnabledToggled(bool checked);
    void handleLinePointLeftSizeEnabledToggled(bool checked);
    void deleteSelectedObjectFromSelection();
    void applyObjectQuickFieldEdits();
    void applyScrapProjectionEdit();
    void updateObjectQuickSubtypeChoices();
    void insertVertexFromSelectionPanel();
    void deleteVertexFromSelectionPanel();
    void toggleVertexSmoothFromSelectionPanel();
    void populateScrapScaleFromSourceBounds();
    void applyScrapScaleEdits();
    void handleConfigureObjectSettingsTriggered();
    void handleLineClosedToggled(bool checked);
    void handleLineReversedToggled(bool checked);

private:
    MapEditorTab *owner_ = nullptr;
};
}
