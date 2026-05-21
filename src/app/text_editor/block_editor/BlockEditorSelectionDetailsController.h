#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorSelectionDetailsController final
{
public:
    explicit BlockEditorSelectionDetailsController(TextEditorTab *owner);

    bool loadSelectionDetails(const QString &kind, int lineNumber);

private:
    TextEditorTab *owner_ = nullptr;
};
}
