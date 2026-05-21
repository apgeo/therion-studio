#pragma once

#include <optional>

class QEvent;
class QObject;

namespace TherionStudio
{
class MapEditorTab;

class MapEditorViewportInputController final
{
public:
    explicit MapEditorViewportInputController(MapEditorTab *owner);

    std::optional<bool> handleEvent(QObject *watched, QEvent *event);

private:
    MapEditorTab *owner_ = nullptr;
};
}
