#pragma once

#include <QPointF>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorMoveController final
{
public:
    explicit BlockEditorMoveController(TextEditorTab *owner);

    void moveBlock(int lineNumber, const QPointF &scenePos);

private:
    TextEditorTab *owner_ = nullptr;
};
}
