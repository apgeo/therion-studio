#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorCanvasRebuildController final
{
public:
    explicit BlockEditorCanvasRebuildController(TextEditorTab *owner);

    void rebuildBlocksCanvasFromText();

private:
    TextEditorTab *owner_ = nullptr;
};
}
