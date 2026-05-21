#pragma once

#include <QString>

#include <optional>

namespace TherionStudio
{
class TextEditorTab;

enum class BlockEditorCommandIdFieldMode
{
    None,
    Optional,
    Required,
};

class BlockEditorCommandOptionsDialog final
{
public:
    explicit BlockEditorCommandOptionsDialog(TextEditorTab *owner);

    std::optional<QString> configureLine(const QString &commandName,
                                         const QString &sourceLine,
                                         int lineNumber,
                                         BlockEditorCommandIdFieldMode idFieldMode);

private:
    TextEditorTab *owner_ = nullptr;
};
}
