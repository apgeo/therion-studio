#pragma once

#include <QString>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorToolboxController final
{
public:
    explicit BlockEditorToolboxController(TextEditorTab *owner);

    void populateBlockToolbox();
    void populateBlockToolboxScopeCombo();
    void updateBlockToolboxScopeLabel();
    QString selectedBlockInsertionContextToken() const;

private:
    TextEditorTab *owner_ = nullptr;
};
}
