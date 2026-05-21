#pragma once

#include <QString>

#include <optional>

namespace TherionStudio
{
class TextEditorTab;

class BlockEditorSingleValueCommandDialog final
{
public:
    explicit BlockEditorSingleValueCommandDialog(TextEditorTab *owner);

    std::optional<QString> configureLine(const QString &commandName,
                                         const QString &sourceLine,
                                         int lineNumber);

private:
    TextEditorTab *owner_ = nullptr;
};
}
