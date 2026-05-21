#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorDetailsPaneController final
{
public:
    explicit BlockEditorDetailsPaneController(TextEditorTab *owner);

    void clearDetailsPane();

private:
    TextEditorTab *owner_ = nullptr;
};
}
