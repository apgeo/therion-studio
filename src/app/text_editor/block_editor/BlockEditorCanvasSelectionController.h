#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorCanvasSelectionController final
{
public:
    explicit BlockEditorCanvasSelectionController(TextEditorTab *owner);

    void selectBlockInCanvasAndDetails(int lineNumber);
    void refreshDetailsSelectionFromScene();

private:
    TextEditorTab *owner_ = nullptr;
};
}
