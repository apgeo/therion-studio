#include "../src/app/text_editor/map_editor/MapEditorUndoArbitrationService.h"

#include <QtTest/QtTest>

using namespace TherionStudio;

namespace
{
class MapEditorUndoArbitrationServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void reportsUndoRedoAvailability();
    void resolvesDefaultOwners();
    void honorsPreferredOwners();
    void prioritizesInteractiveUndo();
    void fallsBackThroughUndoOwners();
    void resolvesRedoOwners();
    void executesPreferredOwners();
    void updatesOwnershipState();
};

void MapEditorUndoArbitrationServiceTest::reportsUndoRedoAvailability()
{
    MapEditorUndoAvailability availability;
    QVERIFY2(!MapEditorUndoArbitrationService::canUndo(availability),
             "Undo arbitration should report canUndo=false when no source can undo.");
    QVERIFY2(!MapEditorUndoArbitrationService::canRedo(availability),
             "Undo arbitration should report canRedo=false when no source can redo.");

    availability.hasInteractiveDrawUndo = true;
    QVERIFY2(MapEditorUndoArbitrationService::canUndo(availability),
             "Undo arbitration should report canUndo when interactive draw undo is available.");

    availability = {};
    availability.hasMapRedo = true;
    QVERIFY2(MapEditorUndoArbitrationService::canRedo(availability),
             "Undo arbitration should report canRedo when map redo is available.");

    availability = {};
    availability.hasTextUndo = true;
    availability.hasTextRedo = true;
    QVERIFY2(MapEditorUndoArbitrationService::canUndo(availability)
                 && MapEditorUndoArbitrationService::canRedo(availability),
             "Undo arbitration should report canUndo/canRedo when text editor supports both.");
}

void MapEditorUndoArbitrationServiceTest::resolvesDefaultOwners()
{
    MapEditorUndoAvailability availability;
    QCOMPARE(MapEditorUndoArbitrationService::nextUndoOwner(availability), MapEditorUndoOwner::None);
    QCOMPARE(MapEditorUndoArbitrationService::nextRedoOwner(availability), MapEditorUndoOwner::None);

    availability.hasTextUndo = true;
    QCOMPARE(MapEditorUndoArbitrationService::nextUndoOwner(availability), MapEditorUndoOwner::TextEdit);

    availability.hasMapUndo = true;
    QCOMPARE(MapEditorUndoArbitrationService::nextUndoOwner(availability), MapEditorUndoOwner::MapCommand);

    availability.hasInteractiveDrawUndo = true;
    QCOMPARE(MapEditorUndoArbitrationService::nextUndoOwner(availability),
             MapEditorUndoOwner::InteractiveDraw);

    availability = {};
    availability.hasTextRedo = true;
    QCOMPARE(MapEditorUndoArbitrationService::nextRedoOwner(availability), MapEditorUndoOwner::TextEdit);

    availability.hasMapRedo = true;
    QCOMPARE(MapEditorUndoArbitrationService::nextRedoOwner(availability), MapEditorUndoOwner::MapCommand);
}

void MapEditorUndoArbitrationServiceTest::honorsPreferredOwners()
{
    MapEditorUndoAvailability availability{
        .hasInteractiveDrawUndo = true,
        .hasMapUndo = true,
        .hasTextUndo = true,
        .preferredUndoOwner = MapEditorUndoOwner::TextEdit};
    QCOMPARE(MapEditorUndoArbitrationService::nextUndoOwner(availability),
             MapEditorUndoOwner::InteractiveDraw);

    availability.hasInteractiveDrawUndo = false;
    QCOMPARE(MapEditorUndoArbitrationService::nextUndoOwner(availability), MapEditorUndoOwner::TextEdit);

    availability.preferredUndoOwner = MapEditorUndoOwner::TextEdit;
    availability.hasTextUndo = false;
    QCOMPARE(MapEditorUndoArbitrationService::nextUndoOwner(availability), MapEditorUndoOwner::MapCommand);

    availability = {
        .hasMapRedo = true,
        .hasTextRedo = true,
        .preferredRedoOwner = MapEditorUndoOwner::TextEdit};
    QCOMPARE(MapEditorUndoArbitrationService::nextRedoOwner(availability), MapEditorUndoOwner::TextEdit);

    availability.preferredRedoOwner = MapEditorUndoOwner::TextEdit;
    availability.hasTextRedo = false;
    QCOMPARE(MapEditorUndoArbitrationService::nextRedoOwner(availability), MapEditorUndoOwner::MapCommand);
}

void MapEditorUndoArbitrationServiceTest::prioritizesInteractiveUndo()
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

    QVERIFY2(MapEditorUndoArbitrationService::triggerUndo(context),
             "Undo arbitration should perform undo when interactive draw undo succeeds.");
    QCOMPARE(interactiveCalls, 1);
    QCOMPARE(mapCalls, 0);
    QCOMPARE(textCalls, 0);
}

void MapEditorUndoArbitrationServiceTest::fallsBackThroughUndoOwners()
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

    QVERIFY2(MapEditorUndoArbitrationService::triggerUndo(context),
             "Undo arbitration should fall back to map undo when interactive draw undo is unavailable.");
    QCOMPARE(interactiveCalls, 1);
    QCOMPARE(mapCalls, 1);
    QCOMPARE(textCalls, 0);

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

    QVERIFY2(MapEditorUndoArbitrationService::triggerUndo(textFallbackContext),
             "Undo arbitration should fall back to text undo when map undo is unavailable.");
    QCOMPARE(mapCalls, 0);
    QCOMPARE(textCalls, 1);
}

void MapEditorUndoArbitrationServiceTest::resolvesRedoOwners()
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

    QVERIFY2(MapEditorUndoArbitrationService::triggerRedo(mapRedoContext),
             "Undo arbitration should perform redo when map redo is available.");
    QCOMPARE(mapCalls, 1);
    QCOMPARE(textCalls, 0);

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

    QVERIFY2(MapEditorUndoArbitrationService::triggerRedo(textRedoContext),
             "Undo arbitration should fall back to text redo when map redo is unavailable.");
    QCOMPARE(mapCalls, 0);
    QCOMPARE(textCalls, 1);
}

void MapEditorUndoArbitrationServiceTest::executesPreferredOwners()
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

    QVERIFY2(MapEditorUndoArbitrationService::triggerUndo(undoContext),
             "Undo arbitration should execute preferred text undo when available.");
    QCOMPARE(mapCalls, 0);
    QCOMPARE(textCalls, 1);

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

    QVERIFY2(MapEditorUndoArbitrationService::triggerRedo(redoContext),
             "Undo arbitration should execute preferred text redo when available.");
    QCOMPARE(mapCalls, 0);
    QCOMPARE(textCalls, 1);
}

void MapEditorUndoArbitrationServiceTest::updatesOwnershipState()
{
    MapEditorUndoOwnershipState state;

    MapEditorUndoArbitrationService::resetOwnership(state, 3);
    QCOMPARE(state.lastMapUndoStackIndex, 3);
    QCOMPARE(state.preferredUndoOwner, MapEditorUndoOwner::None);
    QCOMPARE(state.preferredRedoOwner, MapEditorUndoOwner::None);

    MapEditorUndoArbitrationService::markTextEdited(state);
    QCOMPARE(state.preferredUndoOwner, MapEditorUndoOwner::TextEdit);
    QCOMPARE(state.preferredRedoOwner, MapEditorUndoOwner::None);

    MapEditorUndoArbitrationService::markTextUndoApplied(state);
    QCOMPARE(state.preferredUndoOwner, MapEditorUndoOwner::None);
    QCOMPARE(state.preferredRedoOwner, MapEditorUndoOwner::TextEdit);

    MapEditorUndoArbitrationService::markTextRedoApplied(state);
    QCOMPARE(state.preferredUndoOwner, MapEditorUndoOwner::TextEdit);
    QCOMPARE(state.preferredRedoOwner, MapEditorUndoOwner::None);

    MapEditorUndoArbitrationService::updateMapStackIndex(state, 4);
    QCOMPARE(state.lastMapUndoStackIndex, 4);
    QCOMPARE(state.preferredUndoOwner, MapEditorUndoOwner::MapCommand);
    QCOMPARE(state.preferredRedoOwner, MapEditorUndoOwner::None);

    MapEditorUndoArbitrationService::updateMapStackIndex(state, 3);
    QCOMPARE(state.lastMapUndoStackIndex, 3);
    QCOMPARE(state.preferredUndoOwner, MapEditorUndoOwner::None);
    QCOMPARE(state.preferredRedoOwner, MapEditorUndoOwner::MapCommand);

    MapEditorUndoArbitrationService::markMapCommandApplied(state);
    QCOMPARE(state.preferredUndoOwner, MapEditorUndoOwner::MapCommand);
    QCOMPARE(state.preferredRedoOwner, MapEditorUndoOwner::None);
}
}

int runMapEditorUndoArbitrationServiceTest(int argc, char **argv)
{
    MapEditorUndoArbitrationServiceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "MapEditorUndoArbitrationServiceTest.moc"
