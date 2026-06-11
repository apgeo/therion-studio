#include "MapEditorUndoArbitrationService.h"

namespace TherionStudio
{
namespace
{
bool canExecute(const std::function<bool()> &callback)
{
    return callback && callback();
}

bool ownerAvailableForUndo(MapEditorUndoOwner owner, const MapEditorUndoAvailability &availability)
{
    switch (owner) {
    case MapEditorUndoOwner::InteractiveDraw:
        return availability.hasInteractiveDrawUndo;
    case MapEditorUndoOwner::MapCommand:
        return availability.hasMapUndo;
    case MapEditorUndoOwner::TextEdit:
        return availability.hasTextUndo;
    case MapEditorUndoOwner::None:
        return false;
    }
    return false;
}

bool ownerAvailableForRedo(MapEditorUndoOwner owner, const MapEditorUndoAvailability &availability)
{
    switch (owner) {
    case MapEditorUndoOwner::MapCommand:
        return availability.hasMapRedo;
    case MapEditorUndoOwner::TextEdit:
        return availability.hasTextRedo;
    case MapEditorUndoOwner::InteractiveDraw:
    case MapEditorUndoOwner::None:
        return false;
    }
    return false;
}
}

MapEditorUndoOwner MapEditorUndoArbitrationService::nextUndoOwner(const MapEditorUndoAvailability &availability)
{
    if (availability.hasInteractiveDrawUndo) {
        return MapEditorUndoOwner::InteractiveDraw;
    }
    if (ownerAvailableForUndo(availability.preferredUndoOwner, availability)) {
        return availability.preferredUndoOwner;
    }
    if (availability.hasMapUndo) {
        return MapEditorUndoOwner::MapCommand;
    }
    if (availability.hasTextUndo) {
        return MapEditorUndoOwner::TextEdit;
    }
    return MapEditorUndoOwner::None;
}

MapEditorUndoOwner MapEditorUndoArbitrationService::nextRedoOwner(const MapEditorUndoAvailability &availability)
{
    if (ownerAvailableForRedo(availability.preferredRedoOwner, availability)) {
        return availability.preferredRedoOwner;
    }
    if (availability.hasMapRedo) {
        return MapEditorUndoOwner::MapCommand;
    }
    if (availability.hasTextRedo) {
        return MapEditorUndoOwner::TextEdit;
    }
    return MapEditorUndoOwner::None;
}

bool MapEditorUndoArbitrationService::canUndo(const MapEditorUndoAvailability &availability)
{
    return nextUndoOwner(availability) != MapEditorUndoOwner::None;
}

bool MapEditorUndoArbitrationService::canRedo(const MapEditorUndoAvailability &availability)
{
    return nextRedoOwner(availability) != MapEditorUndoOwner::None;
}

bool MapEditorUndoArbitrationService::triggerUndo(const MapEditorUndoExecutionContext &context)
{
    if (canExecute(context.undoInteractiveDrawStep)) {
        return true;
    }

    const MapEditorUndoAvailability availability{
        .hasMapUndo = canExecute(context.canMapUndo),
        .hasTextUndo = canExecute(context.canTextUndo),
        .preferredUndoOwner = context.preferredUndoOwner};

    switch (nextUndoOwner(availability)) {
    case MapEditorUndoOwner::MapCommand:
        if (!context.undoMapCommand) {
            return false;
        }
        context.undoMapCommand();
        return true;
    case MapEditorUndoOwner::TextEdit:
        if (!context.undoTextEdit) {
            return false;
        }
        context.undoTextEdit();
        return true;
    case MapEditorUndoOwner::InteractiveDraw:
    case MapEditorUndoOwner::None:
        return false;
    }

    return false;
}

bool MapEditorUndoArbitrationService::triggerRedo(const MapEditorUndoExecutionContext &context)
{
    const MapEditorUndoAvailability availability{
        .hasMapRedo = canExecute(context.canMapRedo),
        .hasTextRedo = canExecute(context.canTextRedo),
        .preferredRedoOwner = context.preferredRedoOwner};

    switch (nextRedoOwner(availability)) {
    case MapEditorUndoOwner::MapCommand:
        if (!context.redoMapCommand) {
            return false;
        }
        context.redoMapCommand();
        return true;
    case MapEditorUndoOwner::TextEdit:
        if (!context.redoTextEdit) {
            return false;
        }
        context.redoTextEdit();
        return true;
    case MapEditorUndoOwner::InteractiveDraw:
    case MapEditorUndoOwner::None:
        return false;
    }

    return false;
}
}
