#pragma once

#include "MapEditorViewportInputContext.h"

#include <optional>

class QEvent;
class QObject;

namespace TherionStudio
{
class MapEditorViewportInputController final
{
public:
    explicit MapEditorViewportInputController(MapEditorViewportInputContext context);

    std::optional<bool> handleEvent(QObject *watched, QEvent *event);

private:
    MapEditorInteractiveDrawMode drawMode() const;
    QString tr(const char *text) const;

    MapEditorViewportInputContext context_;
};
}
