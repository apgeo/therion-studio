#pragma once

#include "MapEditorTab.h"

#include <QModelIndex>

namespace TherionStudio
{
class MapEditorInspectorBackgroundController final
{
public:
    explicit MapEditorInspectorBackgroundController(MapEditorTab *owner);

    void configureInspectorBackgroundLayerTreeColumns();
    void handleInspectorBackgroundLayerSelectionChanged(const QModelIndex &current);
    void handleInspectorBackgroundLayerClicked(const QModelIndex &index);
    void refreshInspectorBackgroundPanel();

private:
    MapEditorTab *owner_ = nullptr;
};
}
