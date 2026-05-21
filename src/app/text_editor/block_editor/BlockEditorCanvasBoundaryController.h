#pragma once

#include <QPointF>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorCanvasBoundaryController final
{
public:
    explicit BlockEditorCanvasBoundaryController(TextEditorTab *owner);

    int resolveEndHintContainerStartLineAtScenePos(const QPointF &scenePos) const;

private:
    TextEditorTab *owner_ = nullptr;
};
}
