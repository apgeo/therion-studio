#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorToolboxDetailsController final
{
public:
    explicit BlockEditorToolboxDetailsController(TextEditorTab *owner);

    void showToolboxCommandDetails(const QString &commandToken);

private:
    TextEditorTab *owner_ = nullptr;
};
}
