#pragma once

namespace TherionStudio
{
class TextEditorTab;

class RawEditorCompletionPopupController final
{
public:
    explicit RawEditorCompletionPopupController(TextEditorTab *owner);

    void triggerCompletionPopup();

private:
    TextEditorTab *owner_ = nullptr;
};
}
