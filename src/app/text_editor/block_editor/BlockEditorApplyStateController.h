#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorApplyStateController final
{
public:
    explicit BlockEditorApplyStateController(TextEditorTab *owner);

    void refreshApplyState();

private:
    TextEditorTab *owner_ = nullptr;
};
}
