#pragma once

namespace TherionStudio
{
class MapEditorTab;

class MapEditorSceneRefreshController final
{
public:
    explicit MapEditorSceneRefreshController(MapEditorTab *owner);

    void buildMapScene();
    void refreshMapScene();
    void refreshMapScenePreservingUndoStack();
    void flushPendingMapSceneRefreshAfterCommand();

private:
    MapEditorTab *owner_ = nullptr;
};
}
