#include "MapEditorTab.h"
#include "MapEditorUndoArbitrationService.h"

#include <QScopedValueRollback>
#include <QUndoStack>

#include "../TextEditorTab.h"

namespace TherionStudio
{
MapEditorUndoOwner MapEditorTab::nextUndoOwner() const
{
    const MapEditorUndoAvailability availability{
        .hasInteractiveDrawUndo = hasUndoableInteractiveDrawStep(),
        .hasMapUndo = undoStack_ != nullptr && undoStack_->canUndo(),
        .hasTextUndo = textEditor_ != nullptr && textEditor_->canUndo(),
        .hasMapRedo = undoStack_ != nullptr && undoStack_->canRedo(),
        .hasTextRedo = textEditor_ != nullptr && textEditor_->canRedo(),
        .preferredUndoOwner = preferredUndoOwner_,
        .preferredRedoOwner = preferredRedoOwner_};
    return MapEditorUndoArbitrationService::nextUndoOwner(availability);
}

MapEditorUndoOwner MapEditorTab::nextRedoOwner() const
{
    const MapEditorUndoAvailability availability{
        .hasInteractiveDrawUndo = hasUndoableInteractiveDrawStep(),
        .hasMapUndo = undoStack_ != nullptr && undoStack_->canUndo(),
        .hasTextUndo = textEditor_ != nullptr && textEditor_->canUndo(),
        .hasMapRedo = undoStack_ != nullptr && undoStack_->canRedo(),
        .hasTextRedo = textEditor_ != nullptr && textEditor_->canRedo(),
        .preferredUndoOwner = preferredUndoOwner_,
        .preferredRedoOwner = preferredRedoOwner_};
    return MapEditorUndoArbitrationService::nextRedoOwner(availability);
}

bool MapEditorTab::canUndo() const
{
    return nextUndoOwner() != MapEditorUndoOwner::None;
}

bool MapEditorTab::canRedo() const
{
    return nextRedoOwner() != MapEditorUndoOwner::None;
}

MapEditorTab::InteractiveDrawMode MapEditorTab::interactiveDrawMode() const
{
    return interactiveDrawState_.mode_;
}

bool MapEditorTab::canCompleteDraftAction() const
{
    const bool mapReady = mapScene_ != nullptr;
    return mapReady && (selectedDraftGeometryItem() != nullptr || hasCompletableInteractiveDrawSession());
}

void MapEditorTab::handleDocumentTextChangedForUndoOwner()
{
    if (mapCommandApplyInProgress_) {
        return;
    }

    preferredUndoOwner_ = MapEditorUndoOwner::TextEdit;
    preferredRedoOwner_ = MapEditorUndoOwner::None;
    updateCommandSurfaceState();
}

void MapEditorTab::handleMapUndoStackIndexChanged(int index)
{
    if (index > lastMapUndoStackIndex_) {
        preferredUndoOwner_ = MapEditorUndoOwner::MapCommand;
        preferredRedoOwner_ = MapEditorUndoOwner::None;
    } else if (index < lastMapUndoStackIndex_) {
        preferredUndoOwner_ = MapEditorUndoOwner::None;
        preferredRedoOwner_ = MapEditorUndoOwner::MapCommand;
    }
    lastMapUndoStackIndex_ = index;
    updateCommandSurfaceState();
}

void MapEditorTab::resetUndoOwnerState()
{
    preferredUndoOwner_ = MapEditorUndoOwner::None;
    preferredRedoOwner_ = MapEditorUndoOwner::None;
    lastMapUndoStackIndex_ = undoStack_ != nullptr ? undoStack_->index() : 0;
    updateCommandSurfaceState();
}

void MapEditorTab::triggerUndo()
{
    handleUndoTriggered();
}

void MapEditorTab::triggerRedo()
{
    handleRedoTriggered();
}

void MapEditorTab::triggerZoomIn()
{
    handleZoomInTriggered();
}

void MapEditorTab::triggerZoomOut()
{
    handleZoomOutTriggered();
}

void MapEditorTab::triggerFit()
{
    handleFitTriggered();
}

void MapEditorTab::triggerFitWithBackground()
{
    handleFitWithBackgroundTriggered();
}

void MapEditorTab::triggerSelectMode()
{
    handleSelectModeTriggered();
}

void MapEditorTab::triggerInsertScrap()
{
    handleInsertScrapTriggered();
}

void MapEditorTab::triggerCompleteDraft()
{
    handleCompleteDraftTriggered();
}

void MapEditorTab::triggerAddPoint()
{
    handleAddPointTriggered();
}

void MapEditorTab::triggerAddLine()
{
    handleAddLineTriggered();
}

void MapEditorTab::triggerAddFreehandLine()
{
    handleAddFreehandLineTriggered();
}

void MapEditorTab::triggerAddArea()
{
    handleAddAreaTriggered();
}

void MapEditorTab::triggerSmartArea()
{
    handleSmartAreaTriggered();
}

bool MapEditorTab::isInsertModeActive() const
{
    return interactiveDrawState_.mode_ != InteractiveDrawMode::None;
}

void MapEditorTab::handleUndoTriggered()
{
    const MapEditorUndoExecutionContext context{
        .preferredUndoOwner = preferredUndoOwner_,
        .undoInteractiveDrawStep = [this]() { return undoInteractiveDrawStep(); },
        .canMapUndo = [this]() { return undoStack_ != nullptr && undoStack_->canUndo(); },
        .undoMapCommand = [this]() {
            if (undoStack_ == nullptr) {
                return;
            }
            const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
            undoStack_->undo();
            flushPendingMapSceneRefreshAfterCommand();
        },
        .canTextUndo = [this]() { return textEditor_ != nullptr && textEditor_->canUndo(); },
        .undoTextEdit = [this]() {
            if (textEditor_ != nullptr) {
                textEditor_->triggerUndo();
                preferredRedoOwner_ = MapEditorUndoOwner::TextEdit;
                preferredUndoOwner_ = MapEditorUndoOwner::None;
            }
        },
        .canMapRedo = {},
        .redoMapCommand = {},
        .canTextRedo = {},
        .redoTextEdit = {}};
    MapEditorUndoArbitrationService::triggerUndo(context);
}

void MapEditorTab::handleRedoTriggered()
{
    const MapEditorUndoExecutionContext context{
        .preferredRedoOwner = preferredRedoOwner_,
        .undoInteractiveDrawStep = {},
        .canMapUndo = {},
        .undoMapCommand = {},
        .canTextUndo = {},
        .undoTextEdit = {},
        .canMapRedo = [this]() { return undoStack_ != nullptr && undoStack_->canRedo(); },
        .redoMapCommand = [this]() {
            if (undoStack_ == nullptr) {
                return;
            }
            const QScopedValueRollback<bool> commandGuard(mapCommandApplyInProgress_, true);
            undoStack_->redo();
            flushPendingMapSceneRefreshAfterCommand();
        },
        .canTextRedo = [this]() { return textEditor_ != nullptr && textEditor_->canRedo(); },
        .redoTextEdit = [this]() {
            if (textEditor_ != nullptr) {
                textEditor_->triggerRedo();
                preferredUndoOwner_ = MapEditorUndoOwner::TextEdit;
                preferredRedoOwner_ = MapEditorUndoOwner::None;
            }
        }};
    MapEditorUndoArbitrationService::triggerRedo(context);
}
}
