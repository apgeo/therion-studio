#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorConfigureController final
{
public:
    explicit BlockEditorConfigureController(TextEditorTab *owner);

    void configureBlock(const QString &kind, int lineNumber);

private:
    TextEditorTab *owner_ = nullptr;
};
}
