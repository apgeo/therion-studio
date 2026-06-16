#pragma once

#include <functional>

namespace TherionStudio
{
enum class MapEditorUndoOwner
{
    None,
    InteractiveDraw,
    MapCommand,
    TextEdit
};

struct MapEditorUndoAvailability
{
    bool hasInteractiveDrawUndo = false;
    bool hasMapUndo = false;
    bool hasTextUndo = false;
    bool hasMapRedo = false;
    bool hasTextRedo = false;
    MapEditorUndoOwner preferredUndoOwner = MapEditorUndoOwner::None;
    MapEditorUndoOwner preferredRedoOwner = MapEditorUndoOwner::None;
};

struct MapEditorUndoExecutionContext
{
    MapEditorUndoOwner preferredUndoOwner = MapEditorUndoOwner::None;
    MapEditorUndoOwner preferredRedoOwner = MapEditorUndoOwner::None;
    std::function<bool()> undoInteractiveDrawStep;
    std::function<bool()> canMapUndo;
    std::function<void()> undoMapCommand;
    std::function<bool()> canTextUndo;
    std::function<void()> undoTextEdit;
    std::function<bool()> canMapRedo;
    std::function<void()> redoMapCommand;
    std::function<bool()> canTextRedo;
    std::function<void()> redoTextEdit;
};

struct MapEditorUndoOwnershipState
{
    int lastMapUndoStackIndex = 0;
    MapEditorUndoOwner preferredUndoOwner = MapEditorUndoOwner::None;
    MapEditorUndoOwner preferredRedoOwner = MapEditorUndoOwner::None;
};

class MapEditorUndoArbitrationService final
{
public:
    static MapEditorUndoOwner nextUndoOwner(const MapEditorUndoAvailability &availability);
    static MapEditorUndoOwner nextRedoOwner(const MapEditorUndoAvailability &availability);
    static bool canUndo(const MapEditorUndoAvailability &availability);
    static bool canRedo(const MapEditorUndoAvailability &availability);
    static bool triggerUndo(const MapEditorUndoExecutionContext &context);
    static bool triggerRedo(const MapEditorUndoExecutionContext &context);
    static void markTextEdited(MapEditorUndoOwnershipState &state);
    static void markTextUndoApplied(MapEditorUndoOwnershipState &state);
    static void markTextRedoApplied(MapEditorUndoOwnershipState &state);
    static void markMapCommandApplied(MapEditorUndoOwnershipState &state);
    static void updateMapStackIndex(MapEditorUndoOwnershipState &state, int index);
    static void resetOwnership(MapEditorUndoOwnershipState &state, int currentMapStackIndex);
};
}
