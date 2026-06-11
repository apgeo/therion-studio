#include "MapEditorUndoArbitrationService.h"

namespace TherionStudio
{
namespace
{
bool canExecute(const std::function<bool()> &callback)
{
    return callback && callback();
}
}

MapEditorUndoOwner MapEditorUndoArbitrationService::nextUndoOwner(const MapEditorUndoAvailability &availability)
{
    if (availability.hasInteractiveDrawUndo) {
        return MapEditorUndoOwner::InteractiveDraw;
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

    if (canExecute(context.canMapUndo)) {
        if (!context.undoMapCommand) {
            return false;
        }
        context.undoMapCommand();
        return true;
    }

    if (canExecute(context.canTextUndo)) {
        if (!context.undoTextEdit) {
            return false;
        }
        context.undoTextEdit();
        return true;
    }

    return false;
}

bool MapEditorUndoArbitrationService::triggerRedo(const MapEditorUndoExecutionContext &context)
{
    if (canExecute(context.canMapRedo)) {
        if (!context.redoMapCommand) {
            return false;
        }
        context.redoMapCommand();
        return true;
    }

    if (canExecute(context.canTextRedo)) {
        if (!context.redoTextEdit) {
            return false;
        }
        context.redoTextEdit();
        return true;
    }

    return false;
}
}