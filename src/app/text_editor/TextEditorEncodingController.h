#pragma once

namespace TherionStudio
{
class TextEditorTab;

class TextEditorEncodingController final
{
public:
    explicit TextEditorEncodingController(TextEditorTab *owner);

    void handleConvertToUtf8Triggered();

private:
    TextEditorTab *owner_ = nullptr;
};
}
