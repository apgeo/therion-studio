#include "MapEditorInputPolicy.h"

namespace TherionStudio
{
MapEditorWheelAction resolveMapEditorWheelAction(bool touchFriendlyControlsEnabled,
                                                 bool hasPreciseScrollingDeltas,
                                                 bool zoomModifierPressed)
{
    if (zoomModifierPressed) {
        return MapEditorWheelAction::Zoom;
    }

    if (touchFriendlyControlsEnabled) {
        return MapEditorWheelAction::Pan;
    }

    return hasPreciseScrollingDeltas ? MapEditorWheelAction::Pan : MapEditorWheelAction::Zoom;
}

bool shouldEnableTouchPanCandidate(bool touchFriendlyControlsEnabled,
                                   bool selectModeActive,
                                   bool primaryPointerInteractionActive)
{
    if (primaryPointerInteractionActive) {
        return false;
    }

    return touchFriendlyControlsEnabled || selectModeActive;
}
}
