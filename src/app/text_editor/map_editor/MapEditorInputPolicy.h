#pragma once

namespace TherionStudio
{
enum class MapEditorWheelAction
{
    Zoom,
    Pan
};

MapEditorWheelAction resolveMapEditorWheelAction(bool touchFriendlyControlsEnabled,
                                                 bool hasPreciseScrollingDeltas,
                                                 bool zoomModifierPressed);

bool shouldEnableTouchPanCandidate(bool touchFriendlyControlsEnabled,
                                   bool selectModeActive,
                                   bool primaryPointerInteractionActive);
}
