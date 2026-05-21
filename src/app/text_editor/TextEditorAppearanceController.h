#pragma once

#include <QColor>

namespace TherionStudio
{
class TextEditorTab;

class TextEditorAppearanceController final
{
public:
    explicit TextEditorAppearanceController(TextEditorTab *owner);

    QColor sourceSurfaceColor() const;
    void handleApplicationAppearanceChanged();

private:
    TextEditorTab *owner_ = nullptr;
};
}
