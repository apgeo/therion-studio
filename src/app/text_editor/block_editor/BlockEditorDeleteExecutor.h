#pragma once

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorDeleteExecutor final
{
public:
    explicit BlockEditorDeleteExecutor(TextEditorTab *owner);

    bool deleteCommandAtLine(int lineNumber);

private:
    TextEditorTab *owner_ = nullptr;
};
}
