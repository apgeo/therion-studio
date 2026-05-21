#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorApplyExecutor final
{
public:
    explicit BlockEditorApplyExecutor(TextEditorTab *owner);

    void applyChanges();

private:
    TextEditorTab *owner_ = nullptr;
};
}
