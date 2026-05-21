#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorOptionArgsController final
{
public:
    explicit BlockEditorOptionArgsController(TextEditorTab *owner);

    void refreshOptionArgumentEditors();

private:
    TextEditorTab *owner_ = nullptr;
};
}
