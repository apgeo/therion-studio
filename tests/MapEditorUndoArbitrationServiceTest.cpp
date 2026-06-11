#include "../src/app/text_editor/map_editor/MapEditorUndoArbitrationService.h"

#include <QCoreApplication>

#include <iostream>

using namespace TherionStudio;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

int runCanUndoCanRedoTest()
{
    MapEditorUndoAvailability availability;
    if (!expect(!MapEditorUndoArbitrationService::canUndo(availability),
                "Undo arbitration should report canUndo=false when no source can undo.")) {
        return 1;
    }
    if (!expect(!MapEditorUndoArbitrationService::canRedo(availability),
                "Undo arbitration should report canRedo=false when no source can redo.")) {
        return 1;
    }

    availability.hasInteractiveDrawUndo = true;
    if (!expect(MapEditorUndoArbitrationService::canUndo(availability),
                "Undo arbitration should report canUndo when interactive draw undo is available.")) {
        return 1;
    }

    availability = {};
    availability.hasMapRedo = true;
    if (!expect(MapEditorUndoArbitrationService::canRedo(availability),
                "Undo arbitration should report canRedo when map redo is available.")) {
        return 1;
    }

    availability = {};
    availability.hasTextUndo = true;
    availability.hasTextRedo = true;
    if (!expect(MapEditorUndoArbitrationService::canUndo(availability)
                && MapEditorUndoArbitrationService::canRedo(availability),
                "Undo arbitration should report canUndo/canRedo when text editor supports both.")) {
        return 1;
    }

    return 0;
}

int runOwnerResolutionTest()
{
    MapEditorUndoAvailability availability;
    if (!expect(MapEditorUndoArbitrationService::nextUndoOwner(availability) == MapEditorUndoOwner::None,
                "Undo owner should resolve to None when no undo source is available.")) {
        return 1;
    }
    if (!expect(MapEditorUndoArbitrationService::nextRedoOwner(availability) == MapEditorUndoOwner::None,
                "Redo owner should resolve to None when no redo source is available.")) {
        return 1;
    }

    availability.hasTextUndo = true;
    if (!expect(MapEditorUndoArbitrationService::nextUndoOwner(availability) == MapEditorUndoOwner::TextEdit,
                "Undo owner should resolve to text when only text undo is available.")) {
        return 1;
    }

    availability.hasMapUndo = true;
    if (!expect(MapEditorUndoArbitrationService::nextUndoOwner(availability) == MapEditorUndoOwner::MapCommand,
                "Undo owner should prefer map command over text undo.")) {
        return 1;
    }

    availability.hasInteractiveDrawUndo = true;
    if (!expect(MapEditorUndoArbitrationService::nextUndoOwner(availability) == MapEditorUndoOwner::InteractiveDraw,
                "Undo owner should prefer interactive draw over map/text undo.")) {
        return 1;
    }

    availability = {};
    availability.hasTextRedo = true;
    if (!expect(MapEditorUndoArbitrationService::nextRedoOwner(availability) == MapEditorUndoOwner::TextEdit,
                "Redo owner should resolve to text when only text redo is available.")) {
        return 1;
    }

    availability.hasMapRedo = true;
    if (!expect(MapEditorUndoArbitrationService::nextRedoOwner(availability) == MapEditorUndoOwner::MapCommand,
                "Redo owner should prefer map command over text redo.")) {
        return 1;
    }

    return 0;
}

int runPreferredOwnerResolutionTest()
{
    MapEditorUndoAvailability availability{
        .hasInteractiveDrawUndo = true,
        .hasMapUndo = true,
        .hasTextUndo = true,
        .preferredUndoOwner = MapEditorUndoOwner::TextEdit};
    if (!expect(MapEditorUndoArbitrationService::nextUndoOwner(availability) == MapEditorUndoOwner::InteractiveDraw,
                "Undo owner should keep interactive draw ahead of preferred map/text owners.")) {
        return 1;
    }

    availability.hasInteractiveDrawUndo = false;
    if (!expect(MapEditorUndoArbitrationService::nextUndoOwner(availability) == MapEditorUndoOwner::TextEdit,
                "Undo owner should honor available preferred text owner over map fallback.")) {
        return 1;
    }

    availability.preferredUndoOwner = MapEditorUndoOwner::TextEdit;
    availability.hasTextUndo = false;
    if (!expect(MapEditorUndoArbitrationService::nextUndoOwner(availability) == MapEditorUndoOwner::MapCommand,
                "Undo owner should fall back when preferred text owner is unavailable.")) {
        return 1;
    }

    availability = {
        .hasMapRedo = true,
        .hasTextRedo = true,
        .preferredRedoOwner = MapEditorUndoOwner::TextEdit};
    if (!expect(MapEditorUndoArbitrationService::nextRedoOwner(availability) == MapEditorUndoOwner::TextEdit,
                "Redo owner should honor available preferred text owner over map fallback.")) {
        return 1;
    }

    availability.preferredRedoOwner = MapEditorUndoOwner::TextEdit;
    availability.hasTextRedo = false;
    if (!expect(MapEditorUndoArbitrationService::nextRedoOwner(availability) == MapEditorUndoOwner::MapCommand,
                "Redo owner should fall back when preferred text redo owner is unavailable.")) {
        return 1;
    }

    return 0;
}

int runUndoPriorityTest()
{
    int interactiveCalls = 0;
    int mapCalls = 0;
    int textCalls = 0;

    const MapEditorUndoExecutionContext context{
        .preferredUndoOwner = MapEditorUndoOwner::TextEdit,
        .undoInteractiveDrawStep = [&interactiveCalls]() {
            ++interactiveCalls;
            return true;
        },
        .canMapUndo = []() { return true; },
        .undoMapCommand = [&mapCalls]() { ++mapCalls; },
        .canTextUndo = []() { return true; },
        .undoTextEdit = [&textCalls]() { ++textCalls; },
        .canMapRedo = {},
        .redoMapCommand = {},
        .canTextRedo = {},
        .redoTextEdit = {}};

    if (!expect(MapEditorUndoArbitrationService::triggerUndo(context),
                "Undo arbitration should perform undo when interactive draw undo succeeds.")) {
        return 1;
    }
    if (!expect(interactiveCalls == 1 && mapCalls == 0 && textCalls == 0,
                "Undo arbitration should prioritize interactive draw undo over map/text undo.")) {
        return 1;
    }

    return 0;
}

int runUndoFallbackOrderTest()
{
    int interactiveCalls = 0;
    int mapCalls = 0;
    int textCalls = 0;

    const MapEditorUndoExecutionContext context{
        .undoInteractiveDrawStep = [&interactiveCalls]() {
            ++interactiveCalls;
            return false;
        },
        .canMapUndo = []() { return true; },
        .undoMapCommand = [&mapCalls]() { ++mapCalls; },
        .canTextUndo = []() { return true; },
        .undoTextEdit = [&textCalls]() { ++textCalls; },
        .canMapRedo = {},
        .redoMapCommand = {},
        .canTextRedo = {},
        .redoTextEdit = {}};

    if (!expect(MapEditorUndoArbitrationService::triggerUndo(context),
                "Undo arbitration should fall back to map undo when interactive draw undo is unavailable.")) {
        return 1;
    }
    if (!expect(interactiveCalls == 1 && mapCalls == 1 && textCalls == 0,
                "Undo arbitration should run map undo before text undo fallback.")) {
        return 1;
    }

    mapCalls = 0;
    textCalls = 0;
    const MapEditorUndoExecutionContext textFallbackContext{
        .undoInteractiveDrawStep = {},
        .canMapUndo = []() { return false; },
        .undoMapCommand = [&mapCalls]() { ++mapCalls; },
        .canTextUndo = []() { return true; },
        .undoTextEdit = [&textCalls]() { ++textCalls; },
        .canMapRedo = {},
        .redoMapCommand = {},
        .canTextRedo = {},
        .redoTextEdit = {}};

    if (!expect(MapEditorUndoArbitrationService::triggerUndo(textFallbackContext),
                "Undo arbitration should fall back to text undo when map undo is unavailable.")) {
        return 1;
    }
    if (!expect(mapCalls == 0 && textCalls == 1,
                "Undo arbitration should execute text undo only after map undo is rejected.")) {
        return 1;
    }

    return 0;
}

int runRedoOrderTest()
{
    int mapCalls = 0;
    int textCalls = 0;

    const MapEditorUndoExecutionContext mapRedoContext{
        .undoInteractiveDrawStep = {},
        .canMapUndo = {},
        .undoMapCommand = {},
        .canTextUndo = {},
        .undoTextEdit = {},
        .canMapRedo = []() { return true; },
        .redoMapCommand = [&mapCalls]() { ++mapCalls; },
        .canTextRedo = []() { return true; },
        .redoTextEdit = [&textCalls]() { ++textCalls; }};

    if (!expect(MapEditorUndoArbitrationService::triggerRedo(mapRedoContext),
                "Undo arbitration should perform redo when map redo is available.")) {
        return 1;
    }
    if (!expect(mapCalls == 1 && textCalls == 0,
                "Undo arbitration should prioritize map redo over text redo.")) {
        return 1;
    }

    mapCalls = 0;
    textCalls = 0;
    const MapEditorUndoExecutionContext textRedoContext{
        .undoInteractiveDrawStep = {},
        .canMapUndo = {},
        .undoMapCommand = {},
        .canTextUndo = {},
        .undoTextEdit = {},
        .canMapRedo = []() { return false; },
        .redoMapCommand = [&mapCalls]() { ++mapCalls; },
        .canTextRedo = []() { return true; },
        .redoTextEdit = [&textCalls]() { ++textCalls; }};

    if (!expect(MapEditorUndoArbitrationService::triggerRedo(textRedoContext),
                "Undo arbitration should fall back to text redo when map redo is unavailable.")) {
        return 1;
    }
    if (!expect(mapCalls == 0 && textCalls == 1,
                "Undo arbitration should execute text redo when map redo is unavailable.")) {
        return 1;
    }

    return 0;
}

int runPreferredExecutionOwnerTest()
{
    int mapCalls = 0;
    int textCalls = 0;

    const MapEditorUndoExecutionContext undoContext{
        .preferredUndoOwner = MapEditorUndoOwner::TextEdit,
        .undoInteractiveDrawStep = {},
        .canMapUndo = []() { return true; },
        .undoMapCommand = [&mapCalls]() { ++mapCalls; },
        .canTextUndo = []() { return true; },
        .undoTextEdit = [&textCalls]() { ++textCalls; },
        .canMapRedo = {},
        .redoMapCommand = {},
        .canTextRedo = {},
        .redoTextEdit = {}};

    if (!expect(MapEditorUndoArbitrationService::triggerUndo(undoContext),
                "Undo arbitration should execute preferred text undo when available.")) {
        return 1;
    }
    if (!expect(mapCalls == 0 && textCalls == 1,
                "Undo arbitration should not execute map undo before preferred text undo.")) {
        return 1;
    }

    mapCalls = 0;
    textCalls = 0;
    const MapEditorUndoExecutionContext redoContext{
        .preferredRedoOwner = MapEditorUndoOwner::TextEdit,
        .undoInteractiveDrawStep = {},
        .canMapUndo = {},
        .undoMapCommand = {},
        .canTextUndo = {},
        .undoTextEdit = {},
        .canMapRedo = []() { return true; },
        .redoMapCommand = [&mapCalls]() { ++mapCalls; },
        .canTextRedo = []() { return true; },
        .redoTextEdit = [&textCalls]() { ++textCalls; }};

    if (!expect(MapEditorUndoArbitrationService::triggerRedo(redoContext),
                "Undo arbitration should execute preferred text redo when available.")) {
        return 1;
    }
    if (!expect(mapCalls == 0 && textCalls == 1,
                "Undo arbitration should not execute map redo before preferred text redo.")) {
        return 1;
    }

    return 0;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (runCanUndoCanRedoTest() != 0) {
        return 1;
    }
    if (runOwnerResolutionTest() != 0) {
        return 1;
    }
    if (runPreferredOwnerResolutionTest() != 0) {
        return 1;
    }
    if (runUndoPriorityTest() != 0) {
        return 1;
    }
    if (runUndoFallbackOrderTest() != 0) {
        return 1;
    }
    if (runRedoOrderTest() != 0) {
        return 1;
    }
    if (runPreferredExecutionOwnerTest() != 0) {
        return 1;
    }

    return 0;
}
