#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorDetailsHelpController final
{
public:
    explicit BlockEditorDetailsHelpController(TextEditorTab *owner);

    void updateHelpForCurrentFocus();

private:
    TextEditorTab *owner_ = nullptr;
};
}
