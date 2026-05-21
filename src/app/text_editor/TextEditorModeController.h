#pragma once

namespace TherionStudio
{
class TextEditorTab;

class TextEditorModeController final
{
public:
    explicit TextEditorModeController(TextEditorTab *owner);

    bool isBlocksModeSupportedForCurrentFile() const;
    void refreshBlocksModeAvailability();
    void setBlocksModeActive(bool active);
    bool ensureEncodingRootDirectiveForBlocks();
    void refreshEditorModeUi();

private:
    TextEditorTab *owner_ = nullptr;
};
}
