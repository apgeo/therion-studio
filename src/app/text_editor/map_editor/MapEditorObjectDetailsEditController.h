#pragma once

#include "MapEditorObjectDetailsContext.h"

namespace TherionStudio
{
class MapEditorObjectDetailsEditController final
{
public:
    explicit MapEditorObjectDetailsEditController(MapEditorObjectDetailsContext context);

    void applyObjectOrientationEdits();
    void handleObjectOrientationValueChanged(double value);
    void handleObjectOrientationEnabledToggled(bool checked);
    void handleLinePointLeftSizeEnabledToggled(bool checked);
    void handleLinePointLeftSizeValueChanged(double value);
    void deleteSelectedObjectFromSelection();
    void applyObjectQuickFieldEdits();
    void applyScrapProjectionEdit();
    void updateObjectQuickSubtypeChoices();
    void insertVertexBeforeFromSelectionPanel();
    void insertVertexAfterFromSelectionPanel();
    void splitLineFromSelectionPanel();
    void deleteVertexFromSelectionPanel();
    void handleLinePointPreviousControlToggled(bool checked);
    void handleLinePointSmoothToggled(bool checked);
    void handleLinePointNextControlToggled(bool checked);
    void applyLinePointFlagsEdits();
    void populateScrapScaleFromSourceBounds();
    void applyScrapScaleEdits();
    void handleConfigureObjectSettingsTriggered();
    void handleLineClosedToggled(bool checked);
    void handleLineReversedToggled(bool checked);

private:
    QString tr(const char *text) const;
    const InspectorSymbolCatalog &inspectorSymbolCatalog() const;
    const MapEditorOrientationApplicabilityByCommand &orientationApplicabilityByCommand() const;

    MapEditorObjectDetailsContext context_;
};
}
